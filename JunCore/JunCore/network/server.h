#pragma once
#include <Windows.h>
#include <thread>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/container/LFStack.h"
#include "../../JunCommon/algorithm/Parser.h"

#define PQCS_SEND	1	// 1 : SendPacket::WSASend() worker 스레드에서 처리, 0 : SendPacket 호출 스레드에서 WSASend() call

//------------------------------
// NetworkLib
//------------------------------
class NetServer
{
public:
	NetServer(const char* systemFile, const char* server);
	virtual ~NetServer();

public:
	// 서버 ON/OFF
	void Start();
	void Stop();

	// 라이브러리 외부 API
	void SendPacket(SessionId sessionId, PacketBuffer* sendPacket);
	void Disconnect(SessionId sessionId);

private:
	friend PacketBuffer;

private:
	enum class NetType : BYTE
	{
		LAN,
		NET,
		NONE,
	};

	enum class PQCS_TYPE : BYTE
	{
		SEND_POST = 1,
		RELEASE_SESSION = 2,
		NONE,
	};

private:
	// 프로토콜
	BYTE protocolCode;
	BYTE privateKey;

	// 네트워크
	NetType netType;
	SOCKET listenSock = INVALID_SOCKET;
	HANDLE h_iocp = INVALID_HANDLE_VALUE;

	// 세션
	DWORD sessionUnique = 0;
	Session* sessionArray;
	DWORD maxSession;
	LFStack<DWORD> sessionIdxStack;

	// 스레드
	WORD maxWorker;
	WORD activeWorker;
	std::thread* workerThreadArr;
	std::thread acceptThread;
	std::thread timeOutThread;

	// 옵션
	bool timeoutFlag;

	// 타임아웃
	DWORD timeoutCycle;
	DWORD timeOut;

	// 모니터링
	DWORD acceptCount	= 0;
	DWORD acceptTPS		= 0;
	DWORD acceptTotal	= 0;
	DWORD recvMsgTPS	= 0;
	DWORD sendMsgTPS	= 0;
	alignas(64) DWORD sessionCount = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

private:
	// Session Manager 어댑터 클래스 (IOCP Worker와 연동용)
	class ServerSessionManager 
	{
	private:
		NetServer* server;
	public:
		ServerSessionManager(NetServer* srv) : server(srv) {}
		
		void HandleSendPost(Session* session) { server->SendPost(session); }
		void HandleReleaseSession(Session* session) { server->ReleaseSession(session); }
		void HandleRecvCompletion(Session* session) {
			if (server->netType == NetType::LAN)
				server->RecvCompletionLan(session);
			else 
				server->RecvCompletionNet(session);
		}
		void HandleSendCompletion(Session* session) { server->SendCompletion(session); }
		void HandleDecrementIOCount(Session* session) { server->DecrementIOCount(session); }
	};

private:
	// 스레드
	void WorkerFunc();
	void AcceptFunc();
	void TimeoutFunc();

	// IO 완료 처리 루틴
	void RecvCompletionLan(Session* session);
	void RecvCompletionNet(Session* session);
	void SendCompletion(Session* session);

	// 세션
	SessionId GetSessionId();
	Session* ValidateSession(SessionId sessionId);
	void IncrementIOCount(Session* session);
	void DecrementIOCount(Session* session);
	void DecrementIOCountPQCS(Session* session);
	void DisconnectSession(Session* session);
	bool ReleaseSession(Session* session);

	// Send/Recv
	bool SendPost(Session* session); // 실질적인 Send 처리 함수
	int	 AsyncSend(Session* session);
	bool AsyncRecv(Session* session);

protected:
	// 라이브러리 사용자가 구현해야 하는 함수
	virtual bool OnConnectionRequest(in_addr ip, WORD port) = 0;
	virtual void OnClientJoin(SessionId sessionId) = 0;
	virtual void OnRecv(SessionId sessionId, PacketBuffer* contentsPacket) = 0;
	virtual void OnClientLeave(SessionId sessionId) = 0;
	virtual void OnServerStop() = 0;
	// virtual void OnError(int errorcode /* (wchar*) */) = 0;
	// virtual void OnSend(sessionId, int sendSize) = 0;         
	// virtual void OnWorkerThreadBegin() = 0;                   
	// virtual void OnWorkerThreadEnd() = 0;                     

public:
	// Getter
	void UpdateTPS();
	DWORD GetSessionCount();
	DWORD GetAcceptTPS();
	DWORD GetAcceptTotal();
	DWORD GetSendTPS();
	DWORD GetRecvTPS();
};

//////////////////////////////
// NetServer Inline Func
//////////////////////////////

inline void NetServer::IncrementIOCount(Session* session) 
{
	session->IncrementIOCount();
}

inline void NetServer::DecrementIOCount(Session* session) 
{
	if (session->DecrementIOCount())
	{
		ReleaseSession(session);
	}
}

inline void NetServer::DecrementIOCountPQCS(Session* session) 
{
	session->DecrementIOCountPQCS();
}

// * 'IO Count == 0' 일 때만 세션을 해제가능. (그렇지 않다면, 다른곳에서 참조중일 발생)
inline void NetServer::DisconnectSession(Session* session) 
{
	session->DisconnectSession();
}

//////////////////////////////
// Getter
//////////////////////////////

inline void NetServer::UpdateTPS() 
{
	acceptTPS = acceptCount;
	acceptCount = 0;

	sendMsgTPS = sendMsgCount;
	sendMsgCount = 0;

	recvMsgTPS = recvMsgCount;
	recvMsgCount = 0;
}

inline DWORD NetServer::GetSessionCount() 
{
	return sessionCount;
}

inline DWORD NetServer::GetAcceptTPS() 
{
	return acceptTPS;
}

inline DWORD NetServer::GetAcceptTotal() 
{
	return acceptTotal;
}

inline DWORD NetServer::GetSendTPS() 
{
	return sendMsgTPS;
}

inline DWORD NetServer::GetRecvTPS() 
{
	return recvMsgTPS;
}