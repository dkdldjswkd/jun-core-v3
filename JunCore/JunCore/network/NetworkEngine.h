#pragma once
#include "../core/WindowsIncludes.h"
#include <type_traits>
#include "NetBase.h"
#include "NetworkPolicy.h"
#include "../../JunCommon/queue/PacketJob.h"

// 템플릿 인자 NetworkPolicy로는 추적이 불편해서 NetworkPolicy의 구현부를 추적하기 위한 전방선언
struct ServerPolicy;
struct ClientPolicy;

//---------------------------------------------------------------------
// NetworkEngine - 템플릿 기반 통합 네트워크 클래스
// Policy-Based Design으로 Server/Client 동작 분리
// 가상 함수 대신 템플릿으로 다형성을 구현하여 런타임 오버헤드를 줄이기 위해,
// 템플릿 클래스를 사용한다.
//---------------------------------------------------------------------
template<typename NetworkPolicy>
class NetworkEngine : public NetBase
{
public:
	NetworkEngine();
	virtual ~NetworkEngine();

private:
	// 정책 기반 데이터
	typename NetworkPolicy::PolicyData policyData;

	// 클라이언트 전용 플레이스홀더 (향후 확장용)
	std::conditional_t<NetworkPolicy::IsServer, int, int> placeholder;
	
	// NetworkManager 참조 (PacketJob 제출용)
	class NetworkManager* networkManager = nullptr;

public:
	// NetBase 순수 가상 함수 구현
	void StartImpl() override;
	void StopImpl() override;

	// 정책 기반 TPS 업데이트
	void UpdateTPS() override;

public:
	// 공통 인터페이스
	void SendPacket(SessionId sessionId, PacketBuffer* sendPacket);
	void Disconnect(SessionId sessionId);

	// 정책별 특화 Getter
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, DWORD>::type GetSessionCount() const;
	
	template<bool Enable = NetworkPolicy::IsServer>  
	typename std::enable_if<Enable, DWORD>::type GetAcceptTPS() const;
	
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, DWORD>::type GetAcceptTotal() const;

	// 클라이언트 전용 인터페이스
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type SendPacket(PacketBuffer* sendPacket);
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, bool>::type Disconnect();

public:
	// === Policy 전용 인터페이스 (friend 제거로 인한 공개 API) ===
	// 주의: 이 함수들은 Policy에서만 호출해야 함
	
	// Policy 데이터 접근
	typename NetworkPolicy::PolicyData& GetPolicyData() { return policyData; }
	const typename NetworkPolicy::PolicyData& GetPolicyData() const { return policyData; }
	
	// ClientPolicy에서 플레이스홀더 접근 (조건부 컴파일)
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, int&>::type GetPlaceholder() { return placeholder; }
	
	// IOCP 관련 인터페이스
	HANDLE GetIOCPHandle() const { return h_iocp; }
	void PostCompletion(DWORD bytes, ULONG_PTR key, LPOVERLAPPED overlapped) {
		PostQueuedCompletionStatus(h_iocp, bytes, key, overlapped);
	}
	
	// 스레드 함수들 (Policy에서 스레드 생성 시 사용)
	void WorkerFunc();
	
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type AcceptFunc();
	
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type TimeoutFunc();
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type ConnectFunc();
	
	// Policy에서 콜백 호출
	void CallOnServerStop() { OnServerStop(); }
	void CallOnClientStop() { OnClientStop(); }
	
	// Policy에서 세션 관련 함수 호출
	bool CallSendPost(Session* session) { return SendPost(session); }
	bool CallReleaseSession(Session* session) { return ReleaseSession(session); }
	
	// NetBase 가상 핸들러 구현 (공유 IOCP용)
	void HandleRecvComplete(Session* session) override { HandleRecvCompletion(session); }
	void HandleSendComplete(Session* session) override { HandleSendCompletion(session); }
	void HandleSessionDisconnect(Session* session) override { session->DisconnectSession(); }
	void HandleDecrementIOCount(Session* session) override { 
		if (session->DecrementIOCount()) {
			ReleaseSession(session);
		}
	}
	
	// PacketJob 제출 구현 - 우선순위별 HandlerPool로 전달
	void SubmitPacketJob(Session* session, PacketBuffer* packet) override;
	
	// NetworkManager 참조 설정
	void SetNetworkManager(class NetworkManager* manager) { networkManager = manager; }

private:
	// 세션 관련 (정책으로 위임)
	SessionId GetSessionId();
	Session* ValidateSession(SessionId sessionId);
	bool ReleaseSession(Session* session);

	// 정책 기반 핸들러들
	void HandleSendPost(Session* session);
	void HandleReleaseSession(Session* session);
	void HandleRecvCompletion(Session* session);
	void HandleSendCompletion(Session* session);

	// IOCP Worker 어댑터 (NetBase::RunWorkerThread용)
	class NetworkEngineSessionManager 
	{
	private:
		NetworkEngine* engine;
	public:
		NetworkEngineSessionManager(NetworkEngine* eng) : engine(eng) {}
		
		void HandleSendPost(Session* session) { engine->HandleSendPost(session); }
		void HandleReleaseSession(Session* session) { engine->HandleReleaseSession(session); }
		void HandleRecvCompletion(Session* session) { engine->HandleRecvCompletion(session); }
		void HandleSendCompletion(Session* session) { engine->HandleSendCompletion(session); }
		void HandleDecrementIOCount(Session* session) { engine->HandleDecrementIOCount(session); }
	};

protected:
	// 파생 클래스에서 구현할 순수 가상 함수들
	virtual bool OnConnectionRequest(in_addr ip, WORD port) = 0;
	virtual void OnClientJoin(SessionId sessionId) = 0;
	virtual void OnRecv(SessionId sessionId, PacketBuffer* contentsPacket) = 0;
	virtual void OnClientLeave(SessionId sessionId) = 0;
	virtual void OnServerStop() = 0;
	
	// 클라이언트 전용 콜백들 - virtual로 선언하여 override 가능
	virtual void OnConnect() {}
	virtual void OnRecv(PacketBuffer* contentsPacket) {}
	virtual void OnDisconnect() {}
	virtual void OnClientStop() {}

private:
	// Send/Recv 관련 (NetBase 메서드 사용)
	bool SendPost(Session* session);
	bool AsyncRecv(Session* session);
	
	// RecvCompletion (정책에 따른 분기)
	void RecvCompletionLan(Session* session);
	void RecvCompletionNet(Session* session);
	void SendCompletion(Session* session);
};

//------------------------------
// NetworkEngine 구현 (템플릿이므로 헤더에 구현)
//------------------------------

template<typename NetworkPolicy>
NetworkEngine<NetworkPolicy>::NetworkEngine()
	: NetBase()
{
	NetworkPolicy::Initialize(this, policyData);
	
	// 하위 호환성: 자동 IOCP 연결 (싱글톤 방식)
	EnsureSingletonIOCP();
}

template<typename NetworkPolicy>
NetworkEngine<NetworkPolicy>::~NetworkEngine()
{
	// PolicyData의 소멸자에서 리소스 정리됨
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::StartImpl()
{
	NetworkPolicy::StartNetwork(this, policyData);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::StopImpl()
{
	NetworkPolicy::StopNetwork(this, policyData);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::UpdateTPS()
{
	// 기본 TPS 업데이트 (NetBase)
	UpdateBaseTPS();
	
	// 정책별 TPS 업데이트
	NetworkPolicy::UpdatePolicyTPS(this, policyData);
}

//template<typename NetworkPolicy>
//void NetworkEngine<NetworkPolicy>::WorkerFunc()
//{
//	NetworkEngineSessionManager sessionManager(this);
//	RunWorkerThread(sessionManager);
//}

// 정책 기반 핸들러들은 단순히 정책으로 위임
template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::HandleSendPost(Session* session)
{
	NetworkPolicy::HandleSendPost(this, policyData, session);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::HandleReleaseSession(Session* session)
{
	NetworkPolicy::HandleReleaseSession(this, policyData, session);
}

template<typename NetworkPolicy>  
void NetworkEngine<NetworkPolicy>::HandleRecvCompletion(Session* session)
{
	if (netType == NetType::LAN)
		RecvCompletionLan(session);
	else 
		RecvCompletionNet(session);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::HandleSendCompletion(Session* session)
{
	SendCompletion(session);
}


// RecvCompletion은 NetBase 메서드 사용
template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::RecvCompletionLan(Session* session)
{
	// 새로운 NetBase 인터페이스: 패킷 조립만 담당, 핸들링은 SubmitPacketJob으로 위임
	OnReceiveCompleteLAN(*session,
						 [this, session]() { session->DisconnectSession(); },
						 [this, session]() { AsyncRecv(session); },
						 [this]() { IncrementRecvCount(); });
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::RecvCompletionNet(Session* session)
{
	// 새로운 NetBase 인터페이스: 패킷 조립만 담당, 핸들링은 SubmitPacketJob으로 위임
	OnReceiveCompleteNET(*session,
						 [this, session]() { session->DisconnectSession(); },
						 [this, session]() { AsyncRecv(session); },
						 [this]() { IncrementRecvCount(); });
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::SendCompletion(Session* session)
{
	 OnSendComplete(*session,
					[this, session]() { SendPost(session); },
					[this](int count) { IncrementSendCount(count); });
}

// 누락된 메서드들 구현
template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, DWORD>::type 
NetworkEngine<NetworkPolicy>::GetSessionCount() const
{
	if constexpr (NetworkPolicy::IsServer) {
		return policyData.sessionCount;
	}
}

template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, DWORD>::type 
NetworkEngine<NetworkPolicy>::GetAcceptTPS() const
{
	if constexpr (NetworkPolicy::IsServer) {
		return policyData.acceptTPS;
	}
}

template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, DWORD>::type 
NetworkEngine<NetworkPolicy>::GetAcceptTotal() const
{
	if constexpr (NetworkPolicy::IsServer) {
		return policyData.acceptTotal;
	}
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::SendPacket(SessionId sessionId, PacketBuffer* sendPacket)
{
	Session* session = ValidateSession(sessionId);
	if (session == nullptr) {
		PacketBuffer::Free(sendPacket);
		return;
	}
	
	// 헤더 설정
	if (netType == NetType::LAN) {
		sendPacket->SetLanHeader();
	} else {
		sendPacket->SetNetHeader(protocolCode, privateKey);
	}
	
	// 세션 SendQ에 추가
	session->sendQ.Enqueue(sendPacket);
	
	// SendPost 실행
	SendPost(session);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::Disconnect(SessionId sessionId)
{
	Session* session = ValidateSession(sessionId);
	if (session != nullptr) {
		session->DisconnectSession();
	}
}

template<typename NetworkPolicy>
SessionId NetworkEngine<NetworkPolicy>::GetSessionId()
{
	return NetworkPolicy::GetSessionId(policyData);
}

template<typename NetworkPolicy>
Session* NetworkEngine<NetworkPolicy>::ValidateSession(SessionId sessionId)
{
	return NetworkPolicy::ValidateSession(policyData, sessionId);
}

template<typename NetworkPolicy>
bool NetworkEngine<NetworkPolicy>::ReleaseSession(Session* session)
{
	return NetworkPolicy::ReleaseSession(policyData, session);
}

// 핵심 네트워크 메서드들 구현
template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, void>::type 
NetworkEngine<NetworkPolicy>::AcceptFunc()
{
	static_assert(NetworkPolicy::IsServer, "AcceptFunc is only available for ServerPolicy");
	
	while (true)
	{
		SOCKET clientSock;
		SOCKADDR_IN clientAddr;
		int addrlen = sizeof(clientAddr);
		
		// Accept 대기
		clientSock = accept(policyData.listenSock, (SOCKADDR*)&clientAddr, &addrlen);
		if (clientSock == INVALID_SOCKET)
		{
			if (WSAGetLastError() == WSAEINTR)
				break; // 서버 종료
			continue;
		}
		
		// Connection 검증
		if (!OnConnectionRequest(clientAddr.sin_addr, ntohs(clientAddr.sin_port)))
		{
			closesocket(clientSock);
			continue;
		}
		
		// 세션 할당
		SessionId newSessionId = GetSessionId();
		if (newSessionId.sessionId == INVALID_SESSION_ID)
		{
			closesocket(clientSock);
			continue;
		}
		
		// 세션 초기화
		Session* session = &policyData.sessionArray[newSessionId.SESSION_INDEX];
		session->Set(clientSock, clientAddr.sin_addr, ntohs(clientAddr.sin_port), newSessionId);
		session->SetIOCP(h_iocp);
		session->SetEngine(this);  // 엔진 포인터 설정
		
		// 클라이언트 소켓을 IOCP에 등록 (핵심!)
		// CompletionKey로 세션 포인터를 전달하여 올바른 핸들러 라우팅
		CreateIoCompletionPort((HANDLE)clientSock, h_iocp, (ULONG_PTR)session, 0);
		
		InterlockedIncrement((LONG*)&policyData.sessionCount);
		InterlockedIncrement((LONG*)&policyData.acceptCount);
		InterlockedIncrement((LONG*)&policyData.acceptTotal);
		
		// 첫 Recv 등록
		AsyncRecv(session);
		
		// 클라이언트 접속 완료
		OnClientJoin(newSessionId);
	}
}

template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, void>::type 
NetworkEngine<NetworkPolicy>::TimeoutFunc()
{
	static_assert(NetworkPolicy::IsServer, "TimeoutFunc is only available for ServerPolicy");
	
	while (true)
	{
		Sleep(policyData.timeoutCycle);
		
		DWORD currentTime = static_cast<DWORD>(GetTickCount64());
		
		// 모든 세션 검사
		for (DWORD i = 0; i < policyData.maxSession; i++)
		{
			Session* session = &policyData.sessionArray[i];
			if (session->sock == INVALID_SOCKET) continue;
			
			// 타임아웃 검사
			if (currentTime - session->lastRecvTime > policyData.timeOut)
			{
				session->DisconnectSession();
			}
		}
	}
}

template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, void>::type 
NetworkEngine<NetworkPolicy>::ConnectFunc()
{
	static_assert(!NetworkPolicy::IsServer, "ConnectFunc is only available for ClientPolicy");
	
	if (policyData.isDummyClient)
	{
		// 더미 클라이언트 모드: 모든 세션 연결
		while (policyData.reconnectFlag)
		{
			NetworkPolicy::ConnectAllSessions(policyData);
			
			// 모든 세션에 IOCP 등록 및 첫 Recv 등록
			for (size_t i = 0; i < policyData.currentSessions; ++i)
			{
				Session* session = &policyData.sessions[i];
				if (session->sock != INVALID_SOCKET)
				{
					// 엔진 포인터 설정
					session->SetEngine(this);
					// CompletionKey로 세션 포인터 사용
					CreateIoCompletionPort((HANDLE)session->sock, h_iocp, (ULONG_PTR)session, 0);
					AsyncRecv(session);
				}
			}
			
			OnConnect();  // 연결 완료 콜백
			
			// 연결 유지 대기
			while (policyData.reconnectFlag && policyData.currentSessions > 0) {
				Sleep(1000);
			}
			
			OnDisconnect();  // 연결 해제 콜백
		}
	}
	else
	{
		// 일반 클라이언트 모드: 단일 세션 (재연결 없음)
		if (policyData.reconnectFlag)
		{
			// 세션 ID 할당
			SessionId newSessionId = GetSessionId();
			if (newSessionId.sessionId == INVALID_SESSION_ID)
			{
				Sleep(3000);
				return;
			}
			
			Session* clientSession = &policyData.sessions[newSessionId.SESSION_INDEX];
			
			// 소켓 생성
			clientSession->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
			if (clientSession->sock == INVALID_SOCKET)
			{
				Sleep(3000);
				return;
			}
			
			// 연결 시도
			SOCKADDR_IN serverAddr;
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(policyData.serverPort);
			serverAddr.sin_addr.s_addr = inet_addr(policyData.serverIP);
			
			if (connect(clientSession->sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
			{
				closesocket(clientSession->sock);
				clientSession->sock = INVALID_SOCKET;
				// 세션 ID 반환
				policyData.availableIndexes.Push(newSessionId.SESSION_INDEX);
				Sleep(3000);
				return;
			}
			
			// IOCP에 등록 - CompletionKey로 세션 포인터 사용
			CreateIoCompletionPort((HANDLE)clientSession->sock, h_iocp, (ULONG_PTR)clientSession, 0);
			
			// 세션 초기화
			clientSession->Set(clientSession->sock, serverAddr.sin_addr, policyData.serverPort, newSessionId);
			clientSession->SetIOCP(h_iocp);
			clientSession->SetEngine(this);  // 엔진 포인터 설정
			InterlockedIncrement((LONG*)&policyData.currentSessions);
			
			// 첫 Recv 등록
			AsyncRecv(clientSession);
			
			// 연결 완료
			OnConnect();
			
			// 연결 유지 대기 - 소켓이 유효한 동안 계속 대기
			while (clientSession->sock != INVALID_SOCKET && policyData.reconnectFlag) {
				Sleep(1000);
			}
			
			// 연결 해제
			OnDisconnect();
		}
	}
}

template<typename NetworkPolicy>
bool NetworkEngine<NetworkPolicy>::SendPost(Session* session)
{
	bool isLan = (netType == NetType::LAN);
	return PostSend(*session, isLan,
		[](Session* s) { s->DisconnectSession(); },
		[](Session* s) { s->DecrementIOCount(); });
}

// 클라이언트 전용 SendPacket 구현 (첫 번째 활성 세션에 전송)
template<typename NetworkPolicy>
template<bool Enable>
typename std::enable_if<Enable, void>::type 
NetworkEngine<NetworkPolicy>::SendPacket(PacketBuffer* sendPacket)
{
	static_assert(!NetworkPolicy::IsServer, "This SendPacket is only for ClientPolicy");
	
	// 첫 번째 활성 세션 찾기
	Session* activeSession = nullptr;
	for (auto& session : policyData.sessions)
	{
		if (session.sock != INVALID_SOCKET && !session.disconnectFlag)
		{
			activeSession = &session;
			break;
		}
	}
	
	if (activeSession == nullptr)
	{
		PacketBuffer::Free(sendPacket);
		return;
	}
	
	// 헤더 설정
	if (netType == NetType::LAN) {
		sendPacket->SetLanHeader();
	} else {
		sendPacket->SetNetHeader(protocolCode, privateKey);
	}
	
	// 세션 SendQ에 추가
	activeSession->sendQ.Enqueue(sendPacket);
	
	// SendPost 실행
	SendPost(activeSession);
}

template<typename NetworkPolicy>
bool NetworkEngine<NetworkPolicy>::AsyncRecv(Session* session)
{
	return PostReceive(*session, [](Session* s) { s->DecrementIOCount(); });
}

//==============================================
// Type aliases and metaprogramming utilities
//==============================================

// Main type aliases
using GameServer = NetworkEngine<ServerPolicy>;
using GameClient = NetworkEngine<ClientPolicy>;

// Compile-time type traits
template<typename T>
struct IsNetworkEngine : std::false_type {};

template<typename Policy>
struct IsNetworkEngine<NetworkEngine<Policy>> : std::true_type {};

template<typename T>
constexpr bool IsNetworkEngine_v = IsNetworkEngine<T>::value;

template<typename T>
constexpr bool IsServerType_v = std::is_same_v<T, GameServer>;

template<typename T>
constexpr bool IsClientType_v = std::is_same_v<T, GameClient>;

// Policy extraction metafunction
template<typename T>
struct ExtractPolicy { using type = void; };

template<typename Policy>
struct ExtractPolicy<NetworkEngine<Policy>> { using type = Policy; };

template<typename T>
using ExtractPolicy_t = typename ExtractPolicy<T>::type;

// SFINAE-based concept simulation
template<typename T>
struct HasServerPolicy : std::bool_constant<
    IsNetworkEngine_v<T> && 
    ExtractPolicy_t<T>::IsServer
> {};

template<typename T>
struct HasClientPolicy : std::bool_constant<
    IsNetworkEngine_v<T> && 
    !ExtractPolicy_t<T>::IsServer
> {};

template<typename T>
constexpr bool HasServerPolicy_v = HasServerPolicy<T>::value;

template<typename T>
constexpr bool HasClientPolicy_v = HasClientPolicy<T>::value;

// Function signature validation
template<typename T, typename = void>
struct HasOnRecvMethod : std::false_type {};

template<typename T>
struct HasOnRecvMethod<T, std::void_t<
    decltype(std::declval<T>().OnRecv(
        std::declval<SessionId>(), 
        std::declval<PacketBuffer*>()
    ))
>> : std::true_type {};

template<typename T>
constexpr bool HasOnRecvMethod_v = HasOnRecvMethod<T>::value;

// Policy validation
template<typename Policy, typename = void>
struct HasPolicyData : std::false_type {};

template<typename Policy>
struct HasPolicyData<Policy, std::void_t<typename Policy::PolicyData>> : std::true_type {};

template<typename Policy>
struct IsPolicyValid : std::bool_constant<
    std::is_class_v<Policy> &&
    std::is_same_v<bool, decltype(Policy::IsServer)> &&
    HasPolicyData<Policy>::value
> {};

template<typename Policy>
constexpr bool IsPolicyValid_v = IsPolicyValid<Policy>::value;

// Utility functions
template<typename T>
static constexpr const char* GetPolicyName() noexcept {
    if constexpr (HasServerPolicy_v<T>) {
        return "ServerPolicy";
    } else if constexpr (HasClientPolicy_v<T>) {
        return "ClientPolicy";
    } else {
        return "UnknownPolicy";
    }
}

template<typename T>
static constexpr size_t GetPolicySize() noexcept {
    return sizeof(typename ExtractPolicy_t<T>::PolicyData);
}

//------------------------------
// SubmitPacketJob 구현 - 패킷을 HandlerPool로 전달
//------------------------------
template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::SubmitPacketJob(Session* session, PacketBuffer* packet)
{
    // 현재는 간단히 직접 처리 (나중에 JobQueue 통합 시 확장 가능)
    SessionId sessionId = session->sessionId;
    
    if constexpr (NetworkPolicy::IsServer) {
        OnRecv(sessionId, packet);
    } else {
        OnRecv(packet);
    }
    
    // TODO: 향후 NetworkManager의 HandlerPool로 Job 제출하도록 확장
    // if (networkManager) {
    //     SessionInfo sessionInfo;
    //     sessionInfo.sessionId.sessionId = sessionId.sessionId;
    //     PacketJob job = PacketJob::CreateRecvJob(sessionInfo, packet);
    //     networkManager->SubmitPacketJob(std::move(job));
    // }
}

//==============================================
// Usage examples
//==============================================

/* 
Server implementation example:

class MyGameServer : public GameServer 
{
public:
    MyGameServer() : GameServer("ServerConfig.ini", "SERVER") {}
    
    // Pure virtual function implementations
    bool OnConnectionRequest(in_addr ip, WORD port) override {
        return true; // Allow all connections
    }
    
    void OnClientJoin(SessionId sessionId) override {
        printf("Client connected: %lld\n", sessionId.sessionId);
    }
    
    void OnRecv(SessionId sessionId, PacketBuffer* packet) override {
        // Handle protocol and send response
        SendPacket(sessionId, responsePacket);
    }
    
    void OnClientLeave(SessionId sessionId) override {
        printf("Client disconnected: %lld\n", sessionId.sessionId);
    }
    
    void OnServerStop() override {
        printf("Server stopped\n");
    }
};

Client implementation example:

class MyGameClient : public GameClient 
{
public:
    MyGameClient() : GameClient("ClientConfig.ini", "CLIENT") {}
    
    // Client-specific callbacks
    void OnConnect() override {
        printf("Connected to server\n");
    }
    
    void OnRecv(PacketBuffer* packet) override {
        // Handle data from server
    }
    
    void OnDisconnect() override {
        printf("Disconnected from server\n");
    }
};

Usage:

int main() {
    MyGameServer server;
    server.Start();
    
    // TPS monitoring
    while (true) {
        server.UpdateTPS();
        printf("AcceptTPS: %d, SendTPS: %d, RecvTPS: %d\n", 
               server.GetAcceptTPS(), server.GetSendTPS(), server.GetRecvTPS());
        Sleep(1000);
    }
}

Key features:
- Policy-based design for server/client separation
- Compile-time type safety with SFINAE
- Zero runtime overhead template metaprogramming  
- Modern C++17 features (if constexpr, std::enable_if)
- Full compatibility with existing NetworkEngine<ServerPolicy>/NetworkEngine<ClientPolicy> API
- Header-only template library
*/