#pragma once
#include "NetBase.h"
#include "Session.h"
#include <atomic>
#include <unordered_set>
#include <string>
#include <memory>

//------------------------------
// Client - 클라이언트 네트워크 엔진
// 하나의 서버 타입에 대해 다중 세션 지원
//------------------------------
class Client : public NetBase
{
public:
    Client(std::shared_ptr<IOCPManager> manager, const char* serverIP, WORD port);
    virtual ~Client();

    // 복사/이동 금지
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

public:
    //------------------------------
    // 세션 연결/해제 인터페이스 (Session* 직접 반환)
    //------------------------------
    Session* Connect();
    void Disconnect(Session* session);
    void DisconnectAll();

    //------------------------------
    // 상태 확인
    //------------------------------
    size_t GetActiveSessionCount() const;
    bool HasActiveSessions() const;

protected:
    //------------------------------
    // 클라이언트 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual void OnConnect(Session* session) = 0;
    virtual void OnDisconnect(Session* session) = 0;

private:
    //------------------------------
    // 서버 연결 정보 (생성자에서 고정)
    //------------------------------
    std::string serverIP;
    WORD serverPort;
    
    //------------------------------
    // 동적 세션 관리 (세션 풀 대신 동적 할당)
    //------------------------------
    std::unordered_set<Session*> activeSessions;
    
    // 내부 헬퍼 함수들
    void CleanupSession(Session* session);
    bool RegisterToIOCP(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline Client::Client(std::shared_ptr<IOCPManager> manager, const char* serverIP, WORD port) 
    : NetBase(manager), serverIP(serverIP), serverPort(port)
{
}

inline Client::~Client()
{
    DisconnectAll();
}

inline Session* Client::Connect()
{
    // 초기화 검사 (패킷 핸들러 등록 여부)
    if (!IsInitialized())
    {
        LOG_ERROR("Must call Initialize() before Connect()!");
        return nullptr;
    }
    
    // 새로운 세션 동적 생성
    Session* session = new Session();
    if (!session)
    {
        LOG_ERROR("Failed to create session");
        return nullptr;
    }
    
    session->IncrementIOCount();
    session->sock_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (session->sock_ == INVALID_SOCKET)
    {
        session->DecrementIOCount();
        CleanupSession(session);
        LOG_ERROR("Failed to create client socket (Error: %d)", WSAGetLastError());
        return nullptr;
    }
    
    // 세션 초기화
    in_addr clientIP = { 0 };
    session->Set(session->sock_, clientIP, 0, this);
    
    // 서버 연결
    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) != 1)
    {
        session->DecrementIOCount();
        CleanupSession(session);
        LOG_ERROR("Invalid server IP address: %s", serverIP.c_str());
        return nullptr;
    }

    if (connect(session->sock_, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
    {
        session->DecrementIOCount();
        CleanupSession(session);
        LOG_ERROR("Failed to connect to server %s:%d (Error: %d)", serverIP.c_str(), serverPort, WSAGetLastError());
        return nullptr;
    }
    
    LOG_INFO("Connected to server %s:%d", serverIP.c_str(), serverPort);

    // IOCP에 SOCKET 등록
    if (!RegisterToIOCP(session))
    {
        LOG_ERROR("Failed to register client socket to IOCP (Error: %d)", GetLastError());
        session->DecrementIOCount();
        CleanupSession(session);
        return nullptr;
    }

    // RECV 등록
    if (!iocpManager->PostAsyncReceive(session))
    {
        session->DecrementIOCount();
        CleanupSession(session);
        return nullptr;
    }

    // 활성 세션에 추가
    activeSessions.insert(session);
    
    OnConnect(session);
    session->DecrementIOCount();
    return session;
}

inline void Client::Disconnect(Session* session)
{
    if (!session || activeSessions.find(session) == activeSessions.end()) {
        return;
    }
    
    // 활성 세션에서 제거
    activeSessions.erase(session);
    
    OnDisconnect(session);
    
    // Session은 IOCount가 0이 되면 자동으로 정리됨
    session->DecrementIOCount();
}

inline void Client::DisconnectAll()
{
    // 복사본 만들어서 안전하게 반복
    auto sessionsCopy = activeSessions;
    for (Session* session : sessionsCopy) {
        Disconnect(session);
    }
}

inline size_t Client::GetActiveSessionCount() const
{
    return activeSessions.size();
}

inline bool Client::HasActiveSessions() const
{
    return !activeSessions.empty();
}



//------------------------------
// 내부 헬퍼 함수들
//------------------------------
inline void Client::CleanupSession(Session* session)
{
    if (session) {
        delete session;
    }
}

inline bool Client::RegisterToIOCP(Session* session)
{
    if (!iocpManager || !session) 
    {
        return false;
    }
    
    return iocpManager->RegisterSocket(session->sock_, session);
}