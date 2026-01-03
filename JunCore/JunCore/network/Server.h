#pragma once
#include "../core/WindowsIncludes.h"
#include "NetBase.h"
#include "IOCPManager.h"
#include "Session.h"
#include "../../JunCommon/container/LFStack.h"
#include "../logic/LogicThread.h"
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <mswsock.h>

// AcceptEx 함수 포인터 타입 정의
typedef BOOL(WINAPI* LPFN_ACCEPTEX)(
    SOCKET sListenSocket,
    SOCKET sAcceptSocket,
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    LPDWORD lpdwBytesReceived,
    LPOVERLAPPED lpOverlapped
);

// GetAcceptExSockaddrs 함수 포인터 타입 정의
typedef VOID(WINAPI* LPFN_GETACCEPTEXSOCKADDRS)(
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    LPSOCKADDR* LocalSockaddr,
    LPINT LocalSockaddrLength,
    LPSOCKADDR* RemoteSockaddr,
    LPINT RemoteSockaddrLength
);

class Server : public NetBase
{
    friend class IOCPManager; // IOCPManager가 private 멤버에 접근할 수 있도록
    
public:
    Server(std::shared_ptr<IOCPManager> manager, int logic_thread_count = 0);
    virtual ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

public:
    //------------------------------
    // 서버 시작/정지 인터페이스
    //------------------------------
    bool StartServer(const char* bindIP, WORD port, DWORD maxSessions = 1000);
    void StopServer();
    bool IsServerRunning() const noexcept { return running.load(); }

    //------------------------------
    // LogicThread 관리
    //------------------------------
    void StartLogicThreads();
    void StopLogicThreads();
    LogicThread* GetLogicThread(int index);
    int GetLogicThreadCount() const { return static_cast<int>(logic_threads_.size()); }

protected:
    //------------------------------
    // 서버 전용 가상함수 - 사용자가 재정의
    //------------------------------
	virtual void OnSessionConnect(User* user) = 0;
    virtual void OnServerStart() {}
    virtual void OnServerStop() {}

private:
    //------------------------------
    // 서버 소켓 (기존 ServerSocketManager 멤버들)
    //------------------------------
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKADDR_IN serverAddr{};

    // Accept 관리
    std::atomic<bool> running{false};

    // AcceptEx 함수 포인터들
    LPFN_ACCEPTEX fnAcceptEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS fnGetAcceptExSockaddrs = nullptr;

    //------------------------------
    // LogicThread 관리
    //------------------------------
    std::vector<std::unique_ptr<LogicThread>> logic_threads_;
    int logic_thread_count_ = 0;

    //------------------------------
    // 내부 메서드들
    //------------------------------
    bool LoadAcceptExFunctions();
    bool PostAcceptEx();
};

//------------------------------
// 인라인 구현
//------------------------------

inline Server::Server(std::shared_ptr<IOCPManager> manager, int logic_thread_count)
    : NetBase(manager), logic_thread_count_(logic_thread_count)
{
    // LogicThread 생성 (시작은 StartLogicThreads()에서)
    logic_threads_.reserve(logic_thread_count_);
    for (int i = 0; i < logic_thread_count_; ++i)
    {
        logic_threads_.push_back(std::make_unique<LogicThread>());
    }
}

inline Server::~Server()
{
    StopServer();
}