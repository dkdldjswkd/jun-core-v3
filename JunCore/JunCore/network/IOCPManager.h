#pragma once
#include "../core/WindowsIncludes.h"
#include "Session.h"
#include "../protocol/message.h"
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

//------------------------------
// IOCPManager - IOCP 이벤트 처리와 패킷 조립 전담
// 아름다운 단일 책임 원칙 구현: "IOCP 이벤트만 처리한다"
//------------------------------
class IOCPManager final
{
    // Builder와 NetBase를 friend로 선언
    friend class Builder;
    friend class NetBase;
    
private:
    // IOCP 리소스
    HANDLE iocpHandle;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> shutdown{false};
    
    // 패킷 조립 정책은 message.h와 packet.h의 상수들을 사용
    
public:
    class Builder {
    private:
        int workerCount = 5;
        
    public:
        Builder& WithWorkerCount(int count) 
        {
            workerCount = count; 
            return *this; 
        }
        
        std::unique_ptr<IOCPManager> Build() 
        {
            return std::unique_ptr<IOCPManager>(new IOCPManager(workerCount));
        }
    };
    
    static Builder Create() { return Builder{}; }

private:
    // Builder를 통해서만 생성될 수 있음
    explicit IOCPManager(int workerCount);
    
public:
    ~IOCPManager();
    
    // 복사/이동 금지 (RAII 리소스)
    IOCPManager(const IOCPManager&) = delete;
    IOCPManager& operator=(const IOCPManager&) = delete;
    IOCPManager(IOCPManager&&) = delete;
    IOCPManager& operator=(IOCPManager&&) = delete;

public:
    //------------------------------
    // IOCP 인터페이스 - 깔끔한 추상화
    //------------------------------
    HANDLE GetHandle() const noexcept { return iocpHandle; }
    bool IsValid() const noexcept { return iocpHandle != INVALID_HANDLE_VALUE; }
    
    // 소켓을 IOCP에 등록
    bool RegisterSocket(SOCKET socket, Session* session);
    
    
    // 종료 신호 전송
    void Shutdown();

    //------------------------------
    // 비동기 I/O 등록 함수들 (NetBase에서 사용)
    //------------------------------
    bool PostAsyncReceive(Session* session);
    void PostAsyncSend(Session* session);

private:
    //------------------------------
    // IOCP Worker Thread - 패킷 조립 전담
    //------------------------------
    void RunWorkerThread();
    
    //------------------------------
    // 패킷 조립 로직 - LAN 모드로 통일
    //------------------------------
    void AssembleLANPackets(Session* session);
    
    //------------------------------
    // IOCP 이벤트 처리
    //------------------------------
    void HandleRecvComplete(Session* session, DWORD ioSize);
    void HandleSendComplete(Session* session);
    void HandleSessionDisconnect(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline IOCPManager::IOCPManager(int workerCount) : iocpHandle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
{
    if (iocpHandle == NULL) 
    {
        throw std::runtime_error("CreateIoCompletionPort failed");
    }
    
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