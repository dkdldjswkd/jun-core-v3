#pragma once
#include <Windows.h>
#include <thread>
#include "NetBase.h"
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// NetworkLib
//------------------------------
class NetClient : public NetBase 
{
public:
	NetClient(const char* systemFile, const char* client);
	virtual ~NetClient();

private:
	friend PacketBuffer;

private:
	// 네트워크 (클라이언트 특화)
	SOCKET clientSock = INVALID_SOCKET;
	char serverIP[16] = { 0, };
	WORD serverPort = 0;

	// 세션
	Session clientSession;

	// 스레드
	std::thread workerThread;
	std::thread connectThread;

	// 옵션
	bool reconnectFlag = true;

	// 모니터링은 NetBase에서 상속

private:
	// Session Manager 어댑터 클래스 (IOCP Worker와 연동용)
	class ClientSessionManager 
	{
	private:
		NetClient* client;
	public:
		ClientSessionManager(NetClient* cli) : client(cli) {}
		
		void HandleSendPost(Session* session) { client->SendPost(); }
		void HandleReleaseSession(Session* session) { client->ReleaseSession(); }
		void HandleRecvCompletion(Session* session) {
			if (client->netType == NetType::LAN)
				client->RecvCompletionLAN();
			else 
				client->RecvCompletionNET();
		}
		void HandleSendCompletion(Session* session) { client->SendCompletion(); }
		void HandleDecrementIOCount(Session* session) { client->DecrementIOCount(); }
	};

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
	// 파서 (클라이언트 특화)
	Parser parser;

public:
	// NetBase 구현
	void StartImpl() override { StartClient(); }
	void StopImpl() override { StopClient(); }

private:
	// 내부 구현 함수들
	void StartClient();
	void StopClient();

public:
	void SendPacket(PacketBuffer* send_packet);
	bool Disconnect();
};

inline void NetClient::DecrementIOCount() 
{
	if (clientSession.DecrementIOCount()) 
	{
		ReleaseSession();
	}
}

inline void NetClient::DecrementIOCountPQCS() 
{
	clientSession.DecrementIOCountPQCS();
}

inline void NetClient::IncrementIOCount() 
{
	clientSession.IncrementIOCount();
}

// * 'IO Count == 0' 일 때만 세션을 정리가능. (그렇지 않다면, 다른스레드 메모리접근 발생)
inline void NetClient::DisconnectSession() 
{
	clientSession.DisconnectSession();
}

////////////////////////////// 
// Getter는 NetBase에서 상속
//////////////////////////////
