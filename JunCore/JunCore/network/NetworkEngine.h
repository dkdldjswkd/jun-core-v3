#pragma once
#include "NetBase.h"
#include "NetworkPolicy.h"
#include "IoCommon.h"
#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// NetworkEngine - 템플릿 기반 통합 네트워크 클래스
// Policy-Based Design으로 Server/Client 동작 분리
//------------------------------
template<typename NetworkPolicy>
class NetworkEngine : public NetBase
{
public:
	NetworkEngine(const char* systemFile, const char* configSection);
	virtual ~NetworkEngine();

private:
	// 정책 기반 데이터
	typename NetworkPolicy::PolicyData policyData;

	// 클라이언트 전용 Parser (ClientPolicy에서만 사용)
	std::conditional_t<NetworkPolicy::IsServer, int, Parser> parser;

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
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, Parser&>::type GetParser();

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
	void HandleDecrementIOCount(Session* session);

	// IOCP Worker 어댑터 (IoCommon::IOCPWorkerLoop용)
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
	
	// 클라이언트 전용 콜백들 (SFINAE로 조건부 컴파일)
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type OnConnect() {}
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type OnRecv(PacketBuffer* contentsPacket) {}
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type OnDisconnect() {}
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type OnClientStop() {}

private:
	// WorkerFunc (IoCommon::IOCPWorkerLoop 사용)
	void WorkerFunc();
	
	// 정책별 특화 스레드 함수들
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type AcceptFunc();
	
	template<bool Enable = NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type TimeoutFunc();
	
	template<bool Enable = !NetworkPolicy::IsServer>
	typename std::enable_if<Enable, void>::type ConnectFunc();
	
	// Send/Recv 관련 (IoCommon 사용)
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
NetworkEngine<NetworkPolicy>::NetworkEngine(const char* systemFile, const char* configSection)
	: NetBase(systemFile, configSection)
{
	NetworkPolicy::Initialize(this, policyData, configSection);
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

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::WorkerFunc()
{
	NetworkEngineSessionManager sessionManager(this);
	IoCommon::IOCPWorkerLoop(h_iocp, sessionManager);
}

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

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::HandleDecrementIOCount(Session* session)
{
	if (session->DecrementIOCount())
	{
		if constexpr (NetworkPolicy::IsServer) {
			ReleaseSession(session);
		} else {
			ReleaseSession(session);  // 클라이언트도 동일하게 처리
		}
	}
}

// RecvCompletion은 IoCommon 사용
template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::RecvCompletionLan(Session* session)
{
	IoCommon::RecvCompletionLan(*session,
		[this, session](PacketBuffer* packet) { 
			if constexpr (NetworkPolicy::IsServer) {
				OnRecv(session->sessionId, packet);
			} else {
				OnRecv(packet);  // 클라이언트는 SessionId 불필요
			}
			PacketBuffer::Free(packet);
		},
		[this, session]() { session->DisconnectSession(); },
		[this, session]() { AsyncRecv(session); },
		[this]() { IncrementRecvCount(); }
	);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::RecvCompletionNet(Session* session)
{
	IoCommon::RecvCompletionNet(*session, protocolCode, privateKey,
		[this, session](PacketBuffer* packet) { 
			if constexpr (NetworkPolicy::IsServer) {
				OnRecv(session->sessionId, packet);
			} else {
				OnRecv(packet);  // 클라이언트는 SessionId 불필요
			}
			PacketBuffer::Free(packet);
		},
		[this, session]() { session->DisconnectSession(); },
		[this, session]() { AsyncRecv(session); },
		[this]() { IncrementRecvCount(); }
	);
}

template<typename NetworkPolicy>
void NetworkEngine<NetworkPolicy>::SendCompletion(Session* session)
{
	IoCommon::SendCompletion(*session,
		[this, session]() { SendPost(session); },
		[this](int count) { IncrementSendCount(count); }
	);
}