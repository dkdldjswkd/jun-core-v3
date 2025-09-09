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

//------------------------------
// Server - 서버 네트워크 엔진
// 사용자는 이 클래스를 상속받아 서버를 구현
//------------------------------
class Server : public NetBase
{
public:
    Server();
    virtual ~Server();

    // 복사/이동 금지
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

public:
    //------------------------------
    // 서버 시작/정지 인터페이스
    //------------------------------
    bool StartServer(const char* bindIP, WORD port, DWORD maxSessions = 1000);
    void StopServer();

    // NetBase 순수 가상 함수 구현
    void Start() override final;
    void Stop() override final;

    //------------------------------
    // 모니터링 인터페이스
    //------------------------------
    DWORD GetCurrentSessionCount() const noexcept { return currentSessionCount.load(); }
    bool IsServerRunning() const noexcept { return running.load(); }

protected:
    //------------------------------
    // 서버 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual bool OnConnectionRequest(in_addr clientIP, WORD clientPort) { return true; }
    virtual void OnServerStart() {}
    virtual void OnServerStop() {}

private:
    //------------------------------
    // 서버 소켓 (기존 ServerSocketManager 멤버들)
    //------------------------------
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKADDR_IN serverAddr{};
    
    // Accept 관리
    std::thread acceptThread;
    std::atomic<bool> running{false};
    
    // 세션 관리
    std::vector<Session> sessions;
    LFStack<DWORD> sessionIndexStack;
    std::atomic<DWORD> currentSessionCount{0};
    
    //------------------------------
    // 내부 메서드들 (기존 ServerSocketManager 메서드들)
    //------------------------------
    void AcceptThreadFunc();
    Session* AllocateSession();
    void InitializeSessions(DWORD maxSessions);
    void CleanupSessions();
    bool OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr);
    void OnClientDisconnect(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline Server::Server() : NetBase()
{
}

inline Server::~Server()
{
    StopServer();
}

inline void Server::Start()
{
    // 기본값으로 서버 시작 (사용자가 StartServer를 직접 호출하는 것을 권장)
    StartServer("0.0.0.0", 7777, 1000);
}

inline void Server::Stop()
{
    StopServer();
}