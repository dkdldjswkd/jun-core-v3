#pragma once
#include "NetBase.h"
#include "Session.h"
#include <atomic>
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

protected:
    //------------------------------
    // 클라이언트 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual void OnConnect(Session* session) = 0;

private:
    //------------------------------
    // 서버 연결 정보 (생성자에서 고정)
    //------------------------------
    std::string serverIP;
    WORD serverPort;
    
    
    // 내부 헬퍼 함수들
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
    // 현재 Client는 동적 세션 관리이므로 자동 정리됨
}

inline Session* Client::Connect()
{
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
        LOG_ERROR("Invalid server IP address: %s", serverIP.c_str());
        return nullptr;
    }

    if (connect(session->sock_, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
    {
        session->DecrementIOCount();
        LOG_ERROR("Failed to connect to server %s:%d (Error: %d)", serverIP.c_str(), serverPort, WSAGetLastError());
        return nullptr;
    }
    
    LOG_INFO("Connected to server %s:%d", serverIP.c_str(), serverPort);

    // IOCP에 SOCKET 등록
    if (!RegisterToIOCP(session))
    {
        LOG_ERROR("Failed to register client socket to IOCP (Error: %d)", GetLastError());
        session->DecrementIOCount();
        return nullptr;
    }

    // RECV 등록
    if (!session->PostAsyncReceive())
    {
        session->DecrementIOCount();
        return nullptr;
    }
    
    OnConnect(session);
    session->DecrementIOCount();
    return session;
}

//------------------------------
// 내부 헬퍼 함수들
//------------------------------
inline bool Client::RegisterToIOCP(Session* session)
{
    if (!iocpManager || !session) 
    {
        return false;
    }
    
    return iocpManager->RegisterSocket(session->sock_, session);
}