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
	for (;;) 
	{
		DWORD	io_size = 0;
		Session* p_session = 0;
		LPOVERLAPPED p_overlapped = 0;

		BOOL ret_GQCS = GetQueuedCompletionStatus(h_iocp, &io_size, (PULONG_PTR)&p_session, &p_overlapped, INFINITE);

		// 워커 스레드 종료
		if (io_size == 0 && p_session == 0 && p_overlapped == 0) 
		{
			PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
			return;
		}

		// FIN
		if (io_size == 0) 
		{
			if (&p_session->sendOverlapped == p_overlapped)
			{
				////LOG("NetClient", LOG_LEVEL_FATAL, "Zero Byte Send !!");
			}
			goto Decrement_IOCount;
		}
		// PQCS
		else if ((ULONG_PTR)p_overlapped < (ULONG_PTR)PQCS_TYPE::NONE) 
		{
			switch ((PQCS_TYPE)(BYTE)p_overlapped) 
			{
				case PQCS_TYPE::SEND_POST: 
				{
					SendPost();
					goto Decrement_IOCount;
				}

				case PQCS_TYPE::RELEASE_SESSION: 
				{
					ReleaseSession();
					continue;
				}

				default:
					////LOG("NetServer", LOG_LEVEL_FATAL, "PQCS Default");
					break;
			}
		}

		// recv 완료처리
		if (&p_session->recvOverlapped == p_overlapped) 
		{
			if (ret_GQCS) 
			{
				p_session->recvBuf.MoveRear(io_size);
				p_session->lastRecvTime = timeGetTime();
				// 패킷 수신 후 외부 루틴 호출
				if (NetType::LAN == netType) {
					RecvCompletionLAN();
				}
				else {
					RecvCompletionNET();
				}
			}  
			else 
			{
				////LOG("NetClient", LOG_LEVEL_DEBUG, "Overlapped Recv Fail");
			}
		}
		// send 완료처리
		else if (&p_session->sendOverlapped == p_overlapped) 
		{
			if (ret_GQCS) 
			{
				SendCompletion();
			}
			else 
			{
				////LOG("NetClient", LOG_LEVEL_DEBUG, "Overlapped Send Fail");
			}
		}
		else 
		{
			////LOG("NetClient", LOG_LEVEL_FATAL, "GQCS INVALID Overlapped!!");
		}

	Decrement_IOCount:
		DecrementIOCount();
	}
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
	// Send Packet Free
	for (int i = 0; i < clientSession.sendPacketCount; i++) 
	{
		PacketBuffer::Free(clientSession.sendPacketArr[i]);
		InterlockedIncrement(&sendMsgCount);
	}
	clientSession.sendPacketCount = 0;

	// Send Flag OFF
	InterlockedExchange8((char*)&clientSession.sendFlag, false);

	// Send 완료 체크
	if (clientSession.disconnectFlag)	return;
	if (clientSession.sendQ.GetUseCount() <= 0) return;
	SendPost();
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
	// 패킷 처리
	for (;;) 
	{
		int recv_len = clientSession.recvBuf.GetUseSize();
		if (recv_len <= LAN_HEADER_SIZE)
		{
			break;
		}

		LanHeader lanHeader;
		clientSession.recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		// 페이로드 길이 부족 체크
		if (recv_len < lanHeader.len + LAN_HEADER_SIZE)
		{
			break;
		}

		//------------------------------
		// OnRecv (네트워크 레이어 콜백)
		//------------------------------
		PacketBuffer* csContentsPacket = PacketBuffer::Alloc();

		// 수신한 패킷 복사
		clientSession.recvBuf.MoveFront(LAN_HEADER_SIZE);
		clientSession.recvBuf.Dequeue(csContentsPacket->writePos, lanHeader.len);
		csContentsPacket->MoveWp(lanHeader.len);

		// 수신한 패킷 처리
		OnRecv(csContentsPacket);
		InterlockedIncrement(&recvMsgCount);

		PacketBuffer::Free(csContentsPacket);
	}

	//------------------------------
	// Post Recv
	//------------------------------
	if (false == clientSession.disconnectFlag) 
	{
		AsyncRecv();
	}
}

void NetClient::RecvCompletionNET() {
	// 패킷 처리
	for (;;) {
		int recv_len = clientSession.recvBuf.GetUseSize();
		if (recv_len < NET_HEADER_SIZE)
			break;

		// 암호화된 패킷 헤더 확인
		char encryptPacket[200];
		clientSession.recvBuf.Peek(encryptPacket, NET_HEADER_SIZE);

		// 코드 검증
		BYTE code = ((NetHeader*)encryptPacket)->code;
		if (code != protocolCode) {
			////LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong code!!", WSAGetLastError());
			DisconnectSession();
			break;
		}

		// 페이로드 길이 부족 체크
		WORD payload_len = ((NetHeader*)encryptPacket)->len;
		if (recv_len < (NET_HEADER_SIZE + payload_len)) {
			break;
		}

		// Recv Data 패킷 복사
		clientSession.recvBuf.MoveFront(NET_HEADER_SIZE);
		clientSession.recvBuf.Dequeue(encryptPacket + NET_HEADER_SIZE, payload_len);

		// 암호화된 패킷 복호화
		PacketBuffer* decrypt_packet = PacketBuffer::Alloc();
		if (!decrypt_packet->DecryptPacket(encryptPacket, privateKey)) {
			PacketBuffer::Free(decrypt_packet);
			////LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong checksum!!", WSAGetLastError());
			DisconnectSession();
			break;
		}

		// 수신한 패킷 처리
		OnRecv(decrypt_packet);
		InterlockedIncrement(&recvMsgCount);

		// 복호화된 패킷 Free
		PacketBuffer::Free(decrypt_packet);
	}

	//------------------------------
	// Post Recv (수신 요청 등록)
	//------------------------------
	if (!clientSession.disconnectFlag) {
		AsyncRecv();
	}
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