#pragma once
#include "NetBase.h"
#include "Session.h"
#include <atomic>
#include <boost/scope_exit.hpp>
#include <vector>
#include "../../JunCommon/container/LFStack.h"

//------------------------------
// Client - 클라이언트 네트워크 엔진
// 사용자는 이 클래스를 상속받아 클라이언트를 구현
//------------------------------
class Client : public NetBase
{
public:
    Client();
    virtual ~Client();

    // 복사/이동 금지
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

public:
    //------------------------------
    // 클라이언트 연결/해제 인터페이스
    //------------------------------
    bool Connect(const char* serverIP, WORD port);
    void Disconnect();
    
    // NetBase 순수 가상 함수 구현
    void Start() override final;
    void Stop() override final;

    //------------------------------
    // 클라이언트 상태 확인
    //------------------------------
    bool IsConnected() const;
    
    //------------------------------
    // 메시지 송신 (편의 함수)
    //------------------------------
    bool SendPacket(const char* message);
    bool SendPacket(const void* data, int dataSize);

protected:
    //------------------------------
    // 클라이언트 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual void OnDisconnect() {}

    // NetBase의 OnClientJoin/Leave를 내부적으로 사용 (final 제거)
    void OnClientJoin(Session* session) override;
    void OnClientLeave(Session* session) override;

private:
    //------------------------------
    // 세션 풀 관리 (Server와 통일)
    //------------------------------
    std::vector<Session> sessionPool;
    LFStack<DWORD> sessionIndexStack;
    Session* currentSession = nullptr; // 현재 세션을 하나로만 유지하기위해 사용
    
    // 세션 풀 관리 함수 (Server와 동일한 패턴)
    void InitializeSessionPool(DWORD maxSessions = 1);
    void CleanupSessionPool();
    Session* AllocateSession();

    // 소켓 해제 관리
    bool RegisterToIOCP(Session* session);
};

//------------------------------
// 인라인 구현
//------------------------------

inline Client::Client() : NetBase()
{
    InitializeSessionPool(1);  // 1개 세션으로 풀 초기화
}

inline Client::~Client()
{
    Disconnect();
    CleanupSessionPool();
}

inline bool Client::Connect(const char* serverIP, WORD port)
{
    LOG_WARN_RETURN(currentSession == nullptr, false, "Client is already connected!");
	LOG_ERROR_RETURN(IsIOCPManagerAttached(), false, "IOCP Manager is not attached!");

	currentSession = AllocateSession();
	{
		if (!currentSession)
		{
			LOG_ERROR("Failed to allocate session from pool");
			return false;
		}

		currentSession->IncrementIOCount();
		currentSession->sock_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (currentSession->sock_ == INVALID_SOCKET)
		{
			currentSession->DecrementIOCount();
			currentSession = nullptr;
			LOG_ERROR("Failed to create client socket (Error: %d)", WSAGetLastError());
			return false;
		}
		in_addr clientIP = { 0 };
		SessionId sessionId((WORD)(currentSession - &sessionPool[0]), (DWORD)GetTickCount64());
		currentSession->Set(currentSession->sock_, clientIP, 0, sessionId, this);
	}
    
	SOCKADDR_IN serverAddr{};
	{
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) != 1)
		{
			currentSession->DecrementIOCount();
			LOG_ERROR("Invalid server IP address: %s", serverIP);
			return false;
		}
    }

    if (connect(currentSession->sock_, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
    {
		currentSession->DecrementIOCount();
        LOG_ERROR("Failed to connect to server %s:%d (Error: %d)", serverIP, port, WSAGetLastError());
        return false;
	}
	else
	{
		LOG_INFO("Connected to server %s:%d", serverIP, port);
    }

    // IOCP에 SOCKET 등록
	if (!iocpManager->RegisterSocket(currentSession->sock_, currentSession))
	{
		LOG_ERROR("Failed to register client socket to IOCP (Error: %d)", GetLastError());
		currentSession->DecrementIOCount();
        return false;
	}

    // RECV 등록
    if (!iocpManager->PostAsyncReceive(currentSession))
    {
		currentSession->DecrementIOCount();
        return false;
    }

    OnClientJoin(currentSession);
	currentSession->DecrementIOCount();
    return true;
}

inline void Client::Disconnect()
{
    if (currentSession) 
    {
        // OnClientLeave는 IOCP에서 자동 호출됨 - 직접 호출하지 않음
        OnDisconnect();
        
        // Session은 IOCount가 0이 되면 자동으로 Pool에 반환됨
        // 수동 해제 불필요! DecrementIOCount()만 호출하면 됨
        currentSession->DecrementIOCount();
        currentSession = nullptr;
    }
}

inline void Client::Start()
{
    // 클라이언트는 Connect를 직접 호출해야 함
    LOG_INFO("Client ready. Call Connect(serverIP, port) to connect to server.");
}

inline void Client::Stop()
{
    Disconnect();
}

inline bool Client::IsConnected() const
{
    return (currentSession != nullptr && currentSession->sock_ != INVALID_SOCKET);
}

inline bool Client::SendPacket(const char* message)
{
    if (!IsConnected() || !currentSession) 
    {
        LOG_ERROR("Not connected or no session (session=%p)", currentSession);
        return false;
    }
    
    PacketBuffer* packet = PacketBuffer::Alloc();
    int messageLen = (int)strlen(message);
    packet->PutData(message, messageLen);
    
    printf("[EchoClient][SEND] Sending message: '%s' (len=%d)\n", message, messageLen);
    
    bool result = NetBase::SendPacket(currentSession, packet);
    if (result) 
    {
        printf("[EchoClient][SEND] SendPacket returned success\n");
    } 
    else 
    {
        printf("[EchoClient][SEND] SendPacket returned failure\n");
    }
    
    return result;
}

inline bool Client::SendPacket(const void* data, int dataSize)
{
    if (!IsConnected() || !currentSession) {
        return false;
    }
    
    PacketBuffer* packet = PacketBuffer::Alloc();
    packet->PutData((const char*)data, dataSize);
    
    return NetBase::SendPacket(currentSession, packet);
}

inline void Client::OnClientJoin(Session* session)
{
    LOG_DEBUG("OnClientJoin called for session %lld", session->session_id_.sessionId);
}

inline void Client::OnClientLeave(Session* session)
{
    LOG_DEBUG("OnClientLeave called for session %lld", session->session_id_.sessionId);
    // Disconnect()에서 OnDisconnect()가 호출될 것임
}

inline bool Client::RegisterToIOCP(Session* session)
{
    if (!iocpManager || !session) 
    {
        return false;
    }
    
    return iocpManager->RegisterSocket(session->sock_, session);
}

//------------------------------
// 세션 풀 관리 함수들
//------------------------------
inline void Client::InitializeSessionPool(DWORD maxSessions)
{
    sessionPool.resize(maxSessions);
    
    for (int i = maxSessions - 1; i >= 0; i--) 
    {
        sessionIndexStack.Push(i);
        sessionPool[i].SetSessionPool(&sessionPool, &sessionIndexStack);
    }
}

inline void Client::CleanupSessionPool()
{
    if (currentSession) 
    {
        currentSession = nullptr;
    }
    
    sessionPool.clear();
}

inline Session* Client::AllocateSession()
{
    DWORD index;
    if (!sessionIndexStack.Pop(&index)) {
        return nullptr; // 사용 가능한 세션 없음
    }
    
    if (index >= sessionPool.size()) {
        return nullptr; // 범위 초과
    }
    
    return &sessionPool[index];
}