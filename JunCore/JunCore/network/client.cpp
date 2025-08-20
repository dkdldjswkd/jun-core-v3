#include <WS2tcpip.h>
#include <WinSock2.h>
#include <memory.h>
#include <timeapi.h>
#include <cstring>
#include "client.h"
#include "IoCommon.h"
#include "../protocol/message.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

//------------------------------
// Client Func
//------------------------------
NetClient::NetClient(const char* systemFile, const char* server) 
{
	// Read SystemFile
	parser.LoadFile(systemFile);

	// json parser로 변경할것
	protocolCode	= 0xFF;				// parser.GetValue(server, "PROTOCOL_CODE", (int*)&protocolCode);
	privateKey		= 0xFF;				// parser.GetValue(server, "PRIVATE_KEY", (int*)&privateKey);
	netType			= NetType::NET;		// parser.GetValue(server, "NET_TYPE", (int*)&netType);
	strcpy_s(serverIP, "127.0.0.1");	// parser.GetValue(server, "IP", serverIP);
	serverPort		= 7777;				// parser.GetValue(server, "PORT", (int*)&serverPort);

	// Check system
	if (1 < (BYTE)netType)
	{
		//CRASH();
	}

	//////////////////////////////
	// Set Client
	//////////////////////////////

	WSADATA wsaData;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) 
	{
		////LOG("NetClient", LOG_LEVEL_FATAL, "WSAStartup() Eror(%d)", WSAGetLastError());
		//CRASH();
	}

	// Create IOCP
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	if (INVALID_HANDLE_VALUE == h_iocp) 
	{ 
		/*CRASH();*/ 
	}

	// IOCP 핸들을 clientSession에 설정
	clientSession.SetIOCP(h_iocp);

	// Create IOCP Worker Thread
	workerThread = thread([this] { WorkerFunc(); });
}
NetClient::~NetClient() {}

void NetClient::Start() 
{
	connectThread = thread([this]() {ConnectFunc(); });
	////LOG("NetClient", LOG_LEVEL_DEBUG, "Client Start");
}

void NetClient::WorkerFunc()
{
	ClientSessionManager sessionManager(this);
	IoCommon::IOCPWorkerLoop(h_iocp, sessionManager);
}

void NetClient::ConnectFunc() 
{
	// Create Socket
	clientSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == clientSock) 
	{
		////LOG("NetClient", LOG_LEVEL_FATAL, "WSASocket() Eror(%d)", WSAGetLastError());
		//CRASH();
	}

	// BIND IOCP
	CreateIoCompletionPort((HANDLE)clientSock, h_iocp, (ULONG_PTR)&clientSession, 0);

	// Set server address
	sockaddr_in server_address;
	std::memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(serverPort);
	if (inet_pton(AF_INET, serverIP, &server_address.sin_addr) != 1) 
	{
		////LOG("NetClient", LOG_LEVEL_WARN, "inet_pton() Error(%d)", WSAGetLastError());
		return;
	}

	// Try Connect
	while (connect(clientSock, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) 
	{
		////LOG("NetClient", LOG_LEVEL_WARN, "connect() Error(%d)", WSAGetLastError());
		if (!reconnectFlag) {
			closesocket(clientSock);
			return;
		}
		Sleep(1000);
	}

	// Set client
	clientSession.Set(clientSock, server_address.sin_addr, serverPort, 0);

	// Connect Success
	OnConnect();
	AsyncRecv();

	// 연결 이후 I/O Count 증가 (* Release() 방지용 Connect 타이밍에 산 증가량)
	DecrementIOCountPQCS();
}

void NetClient::ReleaseSession()
{
	if (0 != clientSession.ioCount)
	{
		return;
	}

	// * release_flag(0), iocount(0) -> release_flag(1), iocount(0)
	if (0 == InterlockedCompareExchange64((long long*)&clientSession.releaseFlag, 1, 0)) 
	{
		// 리소스 정리 (소켓, 패킷)
		closesocket(clientSession.sock);
		PacketBuffer* packet;
		while (clientSession.sendQ.Dequeue(&packet)) 
		{
			PacketBuffer::Free(packet);
		}
		for (int i = 0; i < clientSession.sendPacketCount; i++) 
		{
			PacketBuffer::Free(clientSession.sendPacketArr[i]);
		}

		// 사용자 리소스 정리
		OnDisconnect();

		// Connect 재연결
		if (reconnectFlag) 
		{
			if (connectThread.joinable()) 
			{
				connectThread.join();
			}
			connectThread = thread([this]() {ConnectFunc(); });
		}
	}
}

void NetClient::SendCompletion()
{
	IoCommon::SendCompletion(clientSession,
		[this]() { SendPost(); },
		[this](int count) { InterlockedAdd((LONG*)&sendMsgCount, count); }
	);
}

// SendQ Enqueue, SendPost
void NetClient::SendPacket(PacketBuffer* send_packet) 
{
	if (false == ValidateSession())
	{
		return;
	}

	if (clientSession.disconnectFlag) 
	{
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::RELEASE_SESSION);
		return;
	}

	// LAN, NET 처리
	if (NetType::LAN == netType) 
	{
		send_packet->SetLanHeader();
		send_packet->IncrementRefCount();
	}
	else 
	{
		send_packet->SetNetHeader(protocolCode, privateKey);
		send_packet->IncrementRefCount();
	}

	clientSession.sendQ.Enqueue(send_packet);
	PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::SEND_POST);
}

// AsyncSend Call 시도
bool NetClient::SendPost() 
{
	// Empty return
	if (clientSession.sendQ.GetUseCount() <= 0)
		return false;

	// Send 1회 체크 (send flag, true 일때 send 중복 방지)
	if (clientSession.sendFlag == true)
	{
		return false;
	}

	if (InterlockedExchange8((char*)&clientSession.sendFlag, true) == true)
	{
		return false;
	}

	// Empty continue
	if (clientSession.sendQ.GetUseCount() <= 0) 
	{
		InterlockedExchange8((char*)&clientSession.sendFlag, false);
		return SendPost();
	}

	AsyncSend();
	return true;
}

// WSASend() call
int NetClient::AsyncSend() {
	const bool isLan = (NetType::LAN == netType);
	auto disconnect = [this](Session*) { DisconnectSession(); };
	auto decIo = [this](Session*) { DecrementIOCount(); };
	return IoCommon::AsyncSend(clientSession, isLan, disconnect, decIo) ? 1 : 0;
}

bool NetClient::AsyncRecv() 
{
	auto decIo = [this](Session*) { DecrementIOCount(); };
	return IoCommon::AsyncRecv(clientSession, decIo);
}

void NetClient::RecvCompletionLAN()
{
	IoCommon::RecvCompletionLan(clientSession,
		[this](PacketBuffer* packet) { 
			OnRecv(packet); 
			PacketBuffer::Free(packet);
		},
		[this]() { DisconnectSession(); },
		[this]() { AsyncRecv(); },
		[this]() { InterlockedIncrement(&recvMsgCount); }
	);
}

void NetClient::RecvCompletionNET()
{
	IoCommon::RecvCompletionNet(clientSession, protocolCode, privateKey,
		[this](PacketBuffer* packet) { 
			OnRecv(packet); 
			PacketBuffer::Free(packet);
		},
		[this]() { DisconnectSession(); },
		[this]() { AsyncRecv(); },
		[this]() { InterlockedIncrement(&recvMsgCount); }
	);
}

bool NetClient::ValidateSession() {
	IncrementIOCount();

	if (true == clientSession.releaseFlag) 
	{
		DecrementIOCount();
		return false;
	}
	return true;
}

bool NetClient::Disconnect() {
	if (false == ValidateSession()) return true;
	DisconnectSession();
	DecrementIOCountPQCS();
	return true;
}

void NetClient::Stop() {
	// Connect Thread 종료
	reconnectFlag = false;
	if (connectThread.joinable()) {
		connectThread.join();
	}

	// 세션 종료
	if (!clientSession.releaseFlag) {
		DisconnectSession();
	}

	// 세션 해제 확인
	while (!clientSession.releaseFlag) {
		Sleep(100);
	}

	// Worker 종료
	PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
	if (workerThread.joinable()) {
		workerThread.join();
	}

	CloseHandle(h_iocp);
	WSACleanup();

	// 클라이언트 종료 콜백
	OnClientStop();
}

//------------------------------
// SessionId
//------------------------------

SessionId NetClient::GetSessionID() {
	static DWORD unique = 1;
	DWORD index = 0;
	SessionId sessionId(index, unique++);
	return sessionId;
}