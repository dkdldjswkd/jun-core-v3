#pragma once
#include "../core/WindowsIncludes.h"
#include "Session.h"
#include "../../JunCommon/timer/SlidingWindowCounter.h"
#include <vector>
#include <thread>
#include <functional>
#include <memory>
#include <atomic>
#include <stdexcept>

enum class PQCS
{
    none            = 0,
    async_recv_fail = 1,
    async_send_fail = 2,
    connect_fail    = 3,
    accept_fail     = 4,
    accept_success  = 5,
    max
};

//------------------------------
// Forward Declarations
//------------------------------
class NetBase;
class Session;

class IOCPManager final
{
    friend class Builder;
    friend class NetBase;
    
private:
    HANDLE iocpHandle;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> shutdown{false};
    
private:
    // 퍼포먼스 모니터링
    bool enableMonitoring = false;
    std::vector<TimeWindowCounter<uint64_t>*> recvCounters;
    std::vector<TimeWindowCounter<uint64_t>*> sendCounters;
    
public:
    class Builder {
    private:
        int workerCount = 5;
        bool enableMonitoring = false;
        
    public:
        Builder& WithWorkerCount(int count) 
        {
            workerCount = count; 
            return *this; 
        }
        
        Builder& WithMonitoring(bool enable = true)
        {
            enableMonitoring = enable;
            return *this;
        }
        
        std::unique_ptr<IOCPManager> Build() 
        {
            return std::unique_ptr<IOCPManager>(new IOCPManager(workerCount, enableMonitoring));
        }
    };
    
    static Builder Create() { return Builder{}; }

private:
    // Builder를 통해서만 생성될 수 있음
    explicit IOCPManager(int workerCount, bool enableMonitoring);
    
public:
    ~IOCPManager();
    
    // 복사/이동 금지
    IOCPManager(const IOCPManager&) = delete;
    IOCPManager& operator=(const IOCPManager&) = delete;
    IOCPManager(IOCPManager&&) = delete;
    IOCPManager& operator=(IOCPManager&&) = delete;

public:
    HANDLE GetHandle() const noexcept { return iocpHandle; }
    bool IsValid() const noexcept { return iocpHandle != INVALID_HANDLE_VALUE; }
    
    // 소켓을 IOCP에 등록
    bool RegisterSocket(SOCKET socket, Session* session);
    
    // 종료 신호 전송
    void Shutdown();
    
    // 네트워크 통계 조회 (모니터링 활성화 시에만 유효)
	double GetRecvBytesPerSecond(int seconds) const;
	double GetSendBytesPerSecond(int seconds) const;
    bool IsMonitoringEnabled() const noexcept { return enableMonitoring; }

private:
    //------------------------------
    // IOCP Worker Thread - 패킷 조립 전담
    //------------------------------
    void RunWorkerThread();
    
    //------------------------------
    // IOCP 이벤트 처리
    //------------------------------
    void HandleRecvComplete(Session* session, DWORD ioSize);
    void HandleSendComplete(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline IOCPManager::IOCPManager(int workerCount, bool enableMonitoring) 
    : iocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
    , enableMonitoring(enableMonitoring)
{
    if (iocpHandle == NULL) 
    {
        throw std::runtime_error("CreateIoCompletionPort failed");
    }
    
    // 통계 카운터 벡터 초기화
    recvCounters.resize(workerCount, nullptr);
    sendCounters.resize(workerCount, nullptr);
    
    // Worker threads 생성
    workerThreads.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i) 
    {
        workerThreads.emplace_back([this]() { RunWorkerThread(); });
    }
}

inline IOCPManager::~IOCPManager()
{
    Shutdown();
}

inline bool IOCPManager::RegisterSocket(SOCKET socket, Session* session)
{
    return CreateIoCompletionPort((HANDLE)socket, iocpHandle, (ULONG_PTR)session, 0) != NULL;
}

inline void IOCPManager::Shutdown()
{
    if (shutdown.exchange(true)) 
    {
        return;
    }
    
    // 모든 워커 스레드에 종료 신호 전송
    for (size_t i = 0; i < workerThreads.size(); ++i) 
    {
        PostQueuedCompletionStatus(iocpHandle, 0, 0, 0);
    }
    
    // 모든 워커 스레드 종료 대기
    for (auto& thread : workerThreads) 
    {
        if (thread.joinable()) 
        {
            thread.join();
        }
    }
    workerThreads.clear();
    
    // IOCP 핸들 정리
    if (iocpHandle != INVALID_HANDLE_VALUE) 
    {
        CloseHandle(iocpHandle);
        iocpHandle = INVALID_HANDLE_VALUE;
    }
}