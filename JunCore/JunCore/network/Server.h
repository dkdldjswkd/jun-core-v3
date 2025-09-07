#pragma once
#include "NetBase.h"
#include "ServerSocketManager.h"
#include <memory>

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
    DWORD GetCurrentSessionCount() const;
    bool IsServerRunning() const;

protected:
    //------------------------------
    // 서버 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual bool OnConnectionRequest(in_addr clientIP, WORD clientPort) { return true; }
    virtual void OnServerStart() {}
    virtual void OnServerStop() {}

private:
    // ServerSocketManager를 내부로 완전히 캡슐화
    std::unique_ptr<ServerSocketManager> socketManager;
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

inline bool Server::StartServer(const char* bindIP, WORD port, DWORD maxSessions)
{
    LOG_ERROR_RETURN(IsIOCPManagerAttached(), false, "IOCP Manager is not attached!");
    
    if (socketManager && socketManager->IsRunning())
    {
        LOG_WARN("Server is already running!");
        return false;
    }

    socketManager = std::make_unique<ServerSocketManager>(iocpManager, this);
    
    if (socketManager->StartServer(bindIP, port, maxSessions)) 
    {
        OnServerStart();
        LOG_INFO("Server started on %s:%d (Max Sessions: %d)", bindIP, port, maxSessions);
        return true;
    } 
    else 
    {
        LOG_ERROR("Failed to start server!");
        socketManager.reset();
        return false;
    }
}

inline void Server::StopServer()
{
    if (socketManager) 
    {
        socketManager->StopServer();
        socketManager.reset();
        OnServerStop();
        LOG_INFO("Server stopped successfully");
    }
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

inline DWORD Server::GetCurrentSessionCount() const
{
    return socketManager ? socketManager->GetCurrentSessionCount() : 0;
}

inline bool Server::IsServerRunning() const
{
    return socketManager && socketManager->IsRunning();
}