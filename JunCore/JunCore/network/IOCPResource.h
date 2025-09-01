#pragma once
#include "../core/WindowsIncludes.h"
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <system_error>

//------------------------------
// IOCPResource - IOCP 리소스 완벽 캡슐화 (RAII)
// 
// 특징:
// - RAII로 IOCP 핸들과 워커 스레드를 자동 관리
// - Builder 패턴으로 안전한 생성
// - 복사/이동 금지로 유일성 보장
// - 예외 안전성 보장
//------------------------------
class IOCPResource final
{
public:
    // Builder 패턴으로만 생성 가능
    class Builder;
    
private:
    HANDLE handle;
    std::vector<std::thread> workers;
    std::atomic<bool> shutdown;
    
    // private 생성자 - Builder를 통해서만 생성 가능
    explicit IOCPResource(HANDLE h, std::vector<std::thread>&& w) 
        : handle(h), workers(std::move(w)), shutdown(false) 
    {
        if (handle == INVALID_HANDLE_VALUE) 
        {
            throw std::invalid_argument("Invalid IOCP handle");
        }
    }
    
public:
    // 소멸자 - RAII로 모든 리소스 정리
    ~IOCPResource() {
        // 1. 종료 신호 설정
        shutdown.store(true, std::memory_order_release);
        
        // 2. 워커 스레드들에 종료 신호 전송
        for (size_t i = 0; i < workers.size(); ++i) {
            if (!PostQueuedCompletionStatus(handle, 0, 0, 0)) {
                // 로그 출력 가능하면 좋지만, 소멸자에서는 예외 던지지 않음
            }
        }
        
        // 3. 모든 워커 스레드 종료 대기
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        // 4. IOCP 핸들 해제
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
    
    // 복사/이동 금지 - 리소스는 유일해야 함
    IOCPResource(const IOCPResource&) = delete;
    IOCPResource& operator=(const IOCPResource&) = delete;
    IOCPResource(IOCPResource&&) = delete;
    IOCPResource& operator=(IOCPResource&&) = delete;
    
    // 안전한 접근자들
    HANDLE GetHandle() const noexcept { 
        return handle; 
    }
    
    bool IsShutdown() const noexcept { 
        return shutdown.load(std::memory_order_acquire); 
    }
    
    size_t GetWorkerCount() const noexcept { 
        return workers.size(); 
    }
    
    // 완료 포트에 신호 전송 (스레드 안전)
    bool PostCompletion(DWORD bytes, ULONG_PTR key, LPOVERLAPPED overlapped) const noexcept {
        if (IsShutdown()) {
            return false;
        }
        return PostQueuedCompletionStatus(handle, bytes, key, overlapped) != 0;
    }
};

//------------------------------
// IOCPResource::Builder - 안전한 IOCP 리소스 생성
//
// 특징:
// - 유효성 검사 후 생성
// - 예외 안전성 보장
// - 설정 검증
//------------------------------
class IOCPResource::Builder
{
private:
    DWORD workerCount = 0;
    std::function<void(HANDLE)> workerFunc;
    
public:
    Builder() = default;
    
    Builder& WithWorkerCount(DWORD count) {
        // 합리적인 상한선 설정
        if (count == 0 || count > 64) 
        { 
            throw std::invalid_argument("Invalid worker count (must be 1-64)");
        }

        workerCount = count;
        return *this;
    }
    
    Builder& WithWorkerFunction(std::function<void(HANDLE)> func) 
    {
        if (!func) 
        {
            throw std::invalid_argument("Worker function cannot be null");
        }

        workerFunc = std::move(func);
        return *this;
    }
    
    std::unique_ptr<IOCPResource> Build() 
    {
        // 필수 설정 검증
        if (workerCount == 0) 
        {
            throw std::logic_error("Worker count must be set");
        }
        if (!workerFunc) 
        {
            throw std::logic_error("Worker function must be set");
        }
        
        // IOCP 생성
        HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, workerCount);
        if (iocp == INVALID_HANDLE_VALUE) 
        {
            DWORD error = GetLastError();
            throw std::system_error(error, std::system_category(), "Failed to create IOCP");
        }
        
        // 워커 스레드 생성 (예외 안전성 보장)
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        
        try 
        {
            for (DWORD i = 0; i < workerCount; ++i) 
            {
                // 워커 함수 캡처 시 복사본 생성하여 안전성 보장
                workers.emplace_back([iocp, func = workerFunc]() {
                    try 
                    {
                        func(iocp);
                    } 
                    catch (...) 
                    {
                        // 워커 스레드에서 예외 발생 시 프로세스 종료 방지
                        // 실제 환경에서는 로깅 필요
                    }
                });
            }
        } 
        catch (...) 
        {
            // 스레드 생성 실패 시 IOCP 정리
            CloseHandle(iocp);
            
            // 생성된 스레드들 정리
            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }
        
        // 성공적으로 생성된 경우 IOCPResource 반환
        // make_unique 대신 new 사용 (private 생성자 때문)
        return std::unique_ptr<IOCPResource>(new IOCPResource(iocp, std::move(workers)));
    }
};

//------------------------------
// IOCPHandle - 경량 IOCP 핸들 래퍼 (약한 참조)
//
// 특징:
// - IOCPResource의 약한 참조 (소유권 없음)
// - 사용 시 유효성 자동 검사
// - 타입 안전성 보장
// - 성능 최적화 (인라인 함수)
//------------------------------
class IOCPHandle final
{
private:
    HANDLE handle;
    const IOCPResource* resource; // 약한 참조 (소유권 없음)
    
public:
    // IOCPResource 참조로 생성
    explicit IOCPHandle(const IOCPResource& res) noexcept
        : handle(res.GetHandle()), resource(&res) 
    {
    }
    
    // 기본 생성자 - 무효한 핸들
    IOCPHandle() noexcept
        : handle(INVALID_HANDLE_VALUE), resource(nullptr) 
    {
    }
    
    // 복사 가능 (약한 참조이므로)
    IOCPHandle(const IOCPHandle&) = default;
    IOCPHandle& operator=(const IOCPHandle&) = default;
    
    // 이동 가능
    IOCPHandle(IOCPHandle&&) = default;
    IOCPHandle& operator=(IOCPHandle&&) = default;
    
    // 안전한 핸들 접근
    HANDLE Get() const {
        if (!IsValid()) {
            throw std::runtime_error("IOCP handle is invalid or resource has been shutdown");
        }
        return handle;
    }
    
    // 암시적 변환 (편의성)
    operator HANDLE() const { 
        return Get(); 
    }
    
    // 완료 포트 신호 전송
    bool PostCompletion(DWORD bytes, ULONG_PTR key, LPOVERLAPPED overlapped) const {
        if (!IsValid()) {
            return false;
        }
        return resource->PostCompletion(bytes, key, overlapped);
    }
    
    // 유효성 검사 (noexcept)
    bool IsValid() const noexcept {
        return resource && 
               !resource->IsShutdown() && 
               handle != INVALID_HANDLE_VALUE;
    }
    
    // 리소스 정보 접근
    size_t GetWorkerCount() const {
        if (!resource) {
            return 0;
        }
        return resource->GetWorkerCount();
    }
    
    // 비교 연산자
    bool operator==(const IOCPHandle& other) const noexcept {
        return handle == other.handle && resource == other.resource;
    }
    
    bool operator!=(const IOCPHandle& other) const noexcept {
        return !(*this == other);
    }
};

//------------------------------
// 편의 함수들
//------------------------------

// IOCPResource 생성 헬퍼
inline std::unique_ptr<IOCPResource> CreateIOCPResource(
    DWORD workerCount, 
    std::function<void(HANDLE)> workerFunc) 
{
    return IOCPResource::Builder()
        .WithWorkerCount(workerCount)
        .WithWorkerFunction(std::move(workerFunc))
        .Build();
}