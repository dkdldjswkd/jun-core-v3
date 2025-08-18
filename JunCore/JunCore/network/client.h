#pragma once
#include <Windows.h>
#include <thread>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// NetworkLib
//------------------------------
class NetClient 
{
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
	// 프로토콜
	BYTE protocolCode;
	BYTE privateKey;

	// 네트워크
	NetType netType;
	SOCKET clientSock = INVALID_SOCKET;
	HANDLE h_iocp = INVALID_HANDLE_VALUE;
	char serverIP[16] = { 0, };
	WORD serverPort = 0;

	// 세션
	Session clientSession;

	// 스레드
	std::thread workerThread;
	std::thread connectThread;

	// 옵션
	bool reconnectFlag = true;

	// 모니터링
	DWORD recvMsgTPS = 0;
	DWORD sendMsgTPS = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

private:
	// 스레드
	void WorkerFunc();
	void ConnectFunc();

	// IO �Ϸ� ���� ��ƾ
	void RecvCompletionLAN();
	void RecvCompletionNET();
	void SendCompletion();

	// 세션
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

	// 세션정리
	void ReleaseSession();

protected:
	// 라이브러리 사용자가 구현해야 하는 함수들
	virtual void OnConnect() = 0;
	virtual void OnRecv(PacketBuffer* csContentsPacket) = 0;
	virtual void OnDisconnect() = 0;
	virtual void OnClientStop() = 0;
	// virtual void OnError(int errorcode /* (wchar*) */) = 0;
	// virtual void OnSend(SessionID, int sendsize) = 0;       
	// virtual void OnWorkerThreadBegin() = 0;                 
	// virtual void OnWorkerThreadEnd() = 0;                   

public:
	// 파서
	Parser parser;

public:
	void Start();
	void Stop();

public:
	void SendPacket(PacketBuffer* send_packet);
	bool Disconnect();

public:
	// 모니터링 Getter
	void UpdateTPS();
	DWORD GetSendTPS();
	DWORD GetRecvTPS();
};

inline void NetClient::DecrementIOCount() 
{
	if (0 == InterlockedDecrement((LONG*)&clientSession.ioCount)) 
	{
		ReleaseSession();
	}
}

inline void NetClient::DecrementIOCountPQCS() 
{
	if (0 == InterlockedDecrement((LONG*)&clientSession.ioCount)) 
	{
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::RELEASE_SESSION);
	}
}

inline void NetClient::IncrementIOCount() 
{
	InterlockedIncrement((LONG*)&clientSession.ioCount);
}

// * 'IO Count == 0' 일 때만 세션을 정리가능. (그렇지 않다면, 다른스레드 메모리접근 발생)
inline void NetClient::DisconnectSession() 
{
	clientSession.disconnectFlag = true;
	CancelIoEx((HANDLE)clientSession.sock, NULL);
}

////////////////////////////// 
// Getter
////////////////////////////// 

inline void NetClient::UpdateTPS() 
{
	sendMsgTPS = sendMsgCount;
	sendMsgCount = 0;

	recvMsgTPS = recvMsgCount;
	recvMsgCount = 0;
}

inline DWORD NetClient::GetRecvTPS() 
{
	return recvMsgTPS;
}

inline DWORD NetClient::GetSendTPS() 
{
	return sendMsgTPS;
}
