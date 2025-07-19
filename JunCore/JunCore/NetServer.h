#pragma once
#include <Windows.h>
#include <thread>
#include "Session.h"
#include "PacketBuffer.h"
#include "../JunCommon/lib/LFStack.h"
#include "../JunCommon/lib/Parser.h"

#define PQCS_SEND	1	// 1 : SendPacket::WSASend() worker ������ ��ȸ, 0 : SendPacket ȣ�� �����忡�� WSASend() call

//------------------------------
// NetworkLib
//------------------------------
class NetServer {
public:
	NetServer(const char* systemFile, const char* server);
	virtual ~NetServer();

private:
	friend PacketBuffer;

private:
	enum class NetType : BYTE {
		LAN,
		NET,
		NONE,
	};

	enum class PQCS_TYPE : BYTE {
		SEND_POST = 1,
		RELEASE_SESSION = 2,
		NONE,
	};

private:
	// ��������
	BYTE protocolCode;
	BYTE privateKey;

	// ��Ʈ��ũ
	NetType netType;
	SOCKET listenSock = INVALID_SOCKET;
	HANDLE h_iocp = INVALID_HANDLE_VALUE;

	// ����
	DWORD sessionUnique = 0;
	Session* sessionArray;
	DWORD maxSession;
	LFStack<DWORD> sessionIdxStack;

	// ������
	WORD maxWorker;
	WORD activeWorker;
	std::thread* workerThreadArr;
	std::thread acceptThread;
	std::thread timeOutThread;

	// �ɼ�
	bool timeoutFlag;

	// Ÿ�Ӿƿ�
	DWORD timeoutCycle;
	DWORD timeOut;

	// ����͸�
	DWORD acceptCount = 0;
	DWORD acceptTPS= 0;
	DWORD acceptTotal = 0;
	DWORD recvMsgTPS = 0;
	DWORD sendMsgTPS = 0;
	alignas(64) DWORD sessionCount = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

public:
	// ��ƿ
	Parser parser;

private:
	// ������
	void WorkerFunc();
	void AcceptFunc();
	void TimeoutFunc();

	// IO �Ϸ� ���� ��ƾ
	void RecvCompletionLan(Session* session);
	void RecvCompletionNet(Session* session);
	void SendCompletion(Session* session);

	// ����
	SessionId GetSessionId();
	Session* ValidateSession(SessionId sessionId);
	void IncrementIOCount(Session* session);
	void DecrementIOCount(Session* session);
	void DecrementIOCountPQCS(Session* session);
	void DisconnectSession(Session* session);
	bool ReleaseSession(Session* session);

	// Send/Recv
	bool SendPost(Session* session);
	int	 AsyncSend(Session* session);
	bool AsyncRecv(Session* session);

protected:
	// ���̺귯�� ����� �� ������ �Ͽ� ���
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
	// ���� ON/OFF
	void Start();
	void Stop();

	// ���̺귯�� ���� API
	void SendPacket(SessionId sessionId, PacketBuffer* sendPacket);
	void Disconnect(SessionId sessionId);

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

inline void NetServer::IncrementIOCount(Session* session) {
	InterlockedIncrement((LONG*)&session->ioCount);
}

inline void NetServer::DecrementIOCount(Session* session) {
	if (0 == InterlockedDecrement((LONG*)&session->ioCount)) {
		ReleaseSession(session);
	}
}

inline void NetServer::DecrementIOCountPQCS(Session* session) {
	if (0 == InterlockedDecrement((LONG*)&session->ioCount)) {
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)session, (LPOVERLAPPED)PQCS_TYPE::RELEASE_SESSION);
	}
}

// * 'IO Count == 0' �� �� �� ������ ����Ұ�. (�׷��� �ʴٸ�, �ٸ����� ���¹��� �߻�)
inline void NetServer::DisconnectSession(Session* session) {
	session->disconnectFlag = true;
	CancelIoEx((HANDLE)session->sock, NULL);
}

//////////////////////////////
// Getter
//////////////////////////////

inline void NetServer::UpdateTPS() {
	acceptTPS = acceptCount;
	acceptCount = 0;

	sendMsgTPS = sendMsgCount;
	sendMsgCount = 0;

	recvMsgTPS = recvMsgCount;
	recvMsgCount = 0;
}

inline DWORD NetServer::GetSessionCount() {
	return sessionCount;
}

inline DWORD NetServer::GetAcceptTPS() {
	return acceptTPS;
}

inline DWORD NetServer::GetAcceptTotal() {
	return acceptTotal;
}

inline DWORD NetServer::GetSendTPS() {
	return sendMsgTPS;
}

inline DWORD NetServer::GetRecvTPS() {
	return recvMsgTPS;
}