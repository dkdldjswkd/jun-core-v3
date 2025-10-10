#pragma once
#include "../core/WindowsIncludes.h"
#include "NetBase.h"
#include "IOCPManager.h"
#include "Session.h"
#include "../../JunCommon/container/LFStack.h"
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
    Server(std::shared_ptr<IOCPManager> manager);
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
    // 내부 메서드들
    //------------------------------
    bool LoadAcceptExFunctions();
    bool PostAcceptEx();
    bool OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr);
};

//------------------------------
// 인라인 구현
//------------------------------

inline Server::Server(std::shared_ptr<IOCPManager> manager) : NetBase(manager)
{
}

inline Server::~Server()
{
    StopServer();
}