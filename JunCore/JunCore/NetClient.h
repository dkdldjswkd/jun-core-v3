#pragma once
#include <Windows.h>
#include <thread>
#include "Session.h"
#include "PacketBuffer.h"
#include "../JunCommon/lib/Parser.h"

//------------------------------
// NetworkLib
//------------------------------
class NetClient {
public:
	NetClient(const char* systemFile, const char* client);
	virtual ~NetClient();

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
	SOCKET clientSock = INVALID_SOCKET;
	HANDLE h_iocp = INVALID_HANDLE_VALUE;
	char serverIP[16] = { 0, };
	WORD serverPort = 0;

	// ����
	Session clientSession;

	// ������
	std::thread workerThread;
	std::thread connectThread;

	// �ɼ�
	bool reconnectFlag = true;

	// ����͸�
	DWORD recvMsgTPS = 0;
	DWORD sendMsgTPS = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

private:
	// ������
	void WorkerFunc();
	void ConnectFunc();

	// IO �Ϸ� ���� ��ƾ
	void RecvCompletionLAN();
	void RecvCompletionNET();
	void SendCompletion();

	// ����
	SessionId GetSessionID();
	bool ValidateSession();
	inline void IncrementIOCount();
	inline void DecrementIOCount();
	inline void DecrementIOCountPQCS();
	inline void DisconnectSession();

	// Send/Recv
	bool SendPost();
	int	 AsyncSend();
	bool AsyncRecv();

	// ������
	void ReleaseSession();

protected:
	// ���̺귯�� ����� �� ������ �Ͽ� ���
	virtual void OnConnect() = 0;
	virtual void OnRecv(PacketBuffer* csContentsPacket) = 0;
	virtual void OnDisconnect() = 0;
	virtual void OnClientStop() = 0;
	// virtual void OnError(int errorcode /* (wchar*) */) = 0;
	// virtual void OnSend(SessionID, int sendsize) = 0;       
	// virtual void OnWorkerThreadBegin() = 0;                 
	// virtual void OnWorkerThreadEnd() = 0;                   

public:
	// ��ƿ
	Parser parser;

public:
	void Start();
	void Stop();

public:
	void SendPacket(PacketBuffer* send_packet);
	bool Disconnect();

public:
	// ����͸� Getter
	void UpdateTPS();
	DWORD GetSendTPS();
	DWORD GetRecvTPS();
};

inline void NetClient::DecrementIOCount() {
	if (0 == InterlockedDecrement((LONG*)&clientSession.ioCount)) {
		ReleaseSession();
	}
}

inline void NetClient::DecrementIOCountPQCS() {
	if (0 == InterlockedDecrement((LONG*)&clientSession.ioCount)) {
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::RELEASE_SESSION);
	}
}

inline void NetClient::IncrementIOCount() {
	InterlockedIncrement((LONG*)&clientSession.ioCount);
}

// * 'IO Count == 0' �� �� �� ������ ����Ұ�. (�׷��� �ʴٸ�, �ٸ����� ���¹��� �߻�)
inline void NetClient::DisconnectSession() {
	clientSession.disconnectFlag = true;
	CancelIoEx((HANDLE)clientSession.sock, NULL);
}

////////////////////////////// 
// Getter
////////////////////////////// 

inline void NetClient::UpdateTPS() {
	sendMsgTPS = sendMsgCount;
	sendMsgCount = 0;

	recvMsgTPS = recvMsgCount;
	recvMsgCount = 0;
}

inline DWORD NetClient::GetRecvTPS() {
	return recvMsgTPS;
}

inline DWORD NetClient::GetSendTPS() {
	return sendMsgTPS;
}
