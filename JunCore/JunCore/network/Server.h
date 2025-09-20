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

class Server : public NetBase
{
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

    //------------------------------
    // 모니터링 인터페이스
    //------------------------------
    DWORD GetCurrentSessionCount() const noexcept { return currentSessionCount.load(); }

protected:
    //------------------------------
    // 서버 전용 가상함수 - 사용자가 재정의
    //------------------------------
	virtual void OnClientJoin(Session* session) = 0;
	virtual void OnClientLeave(Session* session) = 0;
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
    
    // 세션 관리 (포인터 기반으로 변경)
    std::vector<std::unique_ptr<Session>> sessions;
    LFStack<DWORD> sessionIndexStack;
    std::atomic<DWORD> currentSessionCount = 0;
    
    //------------------------------
    // 내부 메서드들
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

inline Server::Server(std::shared_ptr<IOCPManager> manager) : NetBase(manager)
{
}

inline Server::~Server()
{
    StopServer();
}