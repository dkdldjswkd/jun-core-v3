#pragma once
#include "../core/WindowsIncludes.h"
#include "IOCPManager.h"
#include "NetBase.h"
#include "Session.h"
#include "../protocol/message.h"
#include "../../JunCommon/container/LFStack.h"
#include <thread>
#include <atomic>
#include <memory>

//------------------------------
// ServerSocketManager - 서버 소켓 관리 전담
// 단일 책임: "서버 소켓 생성, Accept, 세션 관리"
//------------------------------
class ServerSocketManager final
{
private:
    // 서버 소켓
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKADDR_IN serverAddr{};
    
    // Accept 관리
    std::thread acceptThread;
    std::atomic<bool> running{false};
    
    // IOCP와 핸들러 참조
    std::shared_ptr<IOCPManager> iocpManager;
    NetBase* netBaseHandler = nullptr;  // NetBase로 직접 저장
    
    // 세션 관리
    Session* sessionArray = nullptr;
    LFStack<DWORD> sessionIndexStack;
    DWORD maxSessionCount = 0;
    std::atomic<DWORD> currentSessionCount{0};
    
public:
    ServerSocketManager(std::shared_ptr<IOCPManager> iocp, NetBase* handler);
    ~ServerSocketManager();
    
    // 복사/이동 금지
    ServerSocketManager(const ServerSocketManager&) = delete;
    ServerSocketManager& operator=(const ServerSocketManager&) = delete;
    
public:
    //------------------------------
    // 서버 시작/정지
    //------------------------------
    bool StartServer(const char* bindIP, WORD port, DWORD maxSessions = 1000);
    void StopServer();
    
    //------------------------------
    // 모니터링 인터페이스
    //------------------------------
    DWORD GetCurrentSessionCount() const noexcept { return currentSessionCount.load(); }
    bool IsRunning() const noexcept { return running.load(); }
    
private:
    //------------------------------
    // Accept 루프
    //------------------------------
    void AcceptThreadFunc();
    
    //------------------------------
    // 세션 관리
    //------------------------------
    Session* AllocateSession();
    void ReleaseSession(Session* session);
    void InitializeSessions(DWORD maxSessions);
    void CleanupSessions();
    
    //------------------------------
    // 클라이언트 연결 처리
    //------------------------------
    void OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr);
    void OnClientDisconnect(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline ServerSocketManager::ServerSocketManager(std::shared_ptr<IOCPManager> iocp, NetBase* handler)
    : iocpManager(iocp), netBaseHandler(handler)
{
    if (!iocpManager || !netBaseHandler) {
        throw std::runtime_error("Invalid IOCPManager or NetBase handler");
    }
}

inline ServerSocketManager::~ServerSocketManager()
{
    StopServer();
}