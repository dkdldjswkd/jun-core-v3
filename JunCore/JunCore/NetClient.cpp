#include <WS2tcpip.h>
#include <WinSock2.h>
#include <memory.h>
#include <timeapi.h>
#include <cstring>
#include "NetClient.h"
#include "protocol.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

//------------------------------
// Server Func
//------------------------------
NetClient::NetClient(const char* systemFile, const char* server) 
{
	// Read SystemFile
	parser.LoadFile(systemFile);
	parser.GetValue(server, "PROTOCOL_CODE", (int*)&protocolCode);
	parser.GetValue(server, "PRIVATE_KEY", (int*)&privateKey);
	parser.GetValue(server, "NET_TYPE", (int*)&netType);
	parser.GetValue(server, "IP", serverIP);
	parser.GetValue(server, "PORT", (int*)&serverPort);

	// Check system
	if (1 < (BYTE)netType)
	{
		//CRASH();
	}

	//////////////////////////////
	// Set Client
	//////////////////////////////

	WSADATA wsaData;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		////LOG("NetClient", LOG_LEVEL_FATAL, "WSAStartup() Eror(%d)", WSAGetLastError());
		//CRASH();
	}

	// Create IOCP
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	if (INVALID_HANDLE_VALUE == h_iocp) { /*CRASH();*/ }

	// Create IOCP Worker Thread
	workerThread = thread([this] { WorkerFunc(); });
}
NetClient::~NetClient() {}

void NetClient::Start() {
	connectThread = thread([this]() {ConnectFunc(); });
	////LOG("NetClient", LOG_LEVEL_DEBUG, "Client Start");
}

void NetClient::WorkerFunc() {
	for (;;) {
		DWORD	io_size = 0;
		Session* p_session = 0;
		LPOVERLAPPED p_overlapped = 0;

		BOOL ret_GQCS = GetQueuedCompletionStatus(h_iocp, &io_size, (PULONG_PTR)&p_session, &p_overlapped, INFINITE);

		// ��Ŀ ������ ����
		if (io_size == 0 && p_session == 0 && p_overlapped == 0) {
			PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
			return;
		}
		// FIN
		if (io_size == 0) {
			if (&p_session->sendOverlapped == p_overlapped)
				////LOG("NetClient", LOG_LEVEL_FATAL, "Zero Byte Send !!");
			goto Decrement_IOCount;
		}
		// PQCS
		else if ((ULONG_PTR)p_overlapped < (ULONG_PTR)PQCS_TYPE::NONE) {
			switch ((PQCS_TYPE)(BYTE)p_overlapped) {
			case PQCS_TYPE::SEND_POST: {
				SendPost();
				goto Decrement_IOCount;
			}

			case PQCS_TYPE::RELEASE_SESSION: {
				ReleaseSession();
				continue;
			}

			default:
				////LOG("NetServer", LOG_LEVEL_FATAL, "PQCS Default");
				break;
			}
		}

		// recv �Ϸ�����
		if (&p_session->recvOverlapped == p_overlapped) {
			if (ret_GQCS) {
				p_session->recvBuf.MoveRear(io_size);
				p_session->lastRecvTime = timeGetTime();
				// ���� ��� �ܺ� ��� ����
				if (NetType::LAN == netType) {
					RecvCompletionLAN();
				}
				else {
					RecvCompletionNET();
				}
			}  
			else {
				////LOG("NetClient", LOG_LEVEL_DEBUG, "Overlapped Recv Fail");
			}
		}
		// send �Ϸ�����
		else if (&p_session->sendOverlapped == p_overlapped) {
			if (ret_GQCS) {
				SendCompletion();
			}
			else {
				////LOG("NetClient", LOG_LEVEL_DEBUG, "Overlapped Send Fail");
			}
		}
		else {
			////LOG("NetClient", LOG_LEVEL_FATAL, "GQCS INVALID Overlapped!!");
		}

	Decrement_IOCount:
		DecrementIOCount();
	}
}

void NetClient::ConnectFunc() {
	// Create Socket
	clientSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == clientSock) {
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
	if (inet_pton(AF_INET, serverIP, &server_address.sin_addr) != 1) {
		////LOG("NetClient", LOG_LEVEL_WARN, "inet_pton() Error(%d)", WSAGetLastError());
		return;
	}

	// Try Connect
	while (connect(clientSock, (struct sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
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

	// ���� I/O Count ���� (* Release() ���� Connect �翬�� �� �����)
	DecrementIOCountPQCS();
}

void NetClient::ReleaseSession(){
	if (0 != clientSession.ioCount)
		return;

	// * release_flag(0), iocount(0) -> release_flag(1), iocount(0)
	if (0 == InterlockedCompareExchange64((long long*)&clientSession.releaseFlag, 1, 0)) {
		// ���ҽ� ���� (����, ��Ŷ)
		closesocket(clientSession.sock);
		PacketBuffer* packet;
		while (clientSession.sendQ.Dequeue(&packet)) {
			PacketBuffer::Free(packet);
		}
		for (int i = 0; i < clientSession.sendPacketCount; i++) {
			PacketBuffer::Free(clientSession.sendPacketArr[i]);
		}

		// ����� ���ҽ� ����
		OnDisconnect();

		// Conncet ��õ�
		if (reconnectFlag) {
			if (connectThread.joinable()) {
				connectThread.join();
			}
			connectThread = thread([this]() {ConnectFunc(); });
		}
	}
}

void NetClient::SendCompletion(){
	// Send Packet Free
	for (int i = 0; i < clientSession.sendPacketCount; i++) {
		PacketBuffer::Free(clientSession.sendPacketArr[i]);
		InterlockedIncrement(&sendMsgCount);
	}
	clientSession.sendPacketCount = 0;

	// Send Flag OFF
	InterlockedExchange8((char*)&clientSession.sendFlag, false);

	// Send ���� üũ
	if (clientSession.disconnectFlag)	return;
	if (clientSession.sendQ.GetUseCount() <= 0) return;
	SendPost();
}

// SendQ Enqueue, SendPost
void NetClient::SendPacket(PacketBuffer* send_packet) {
	if (false == ValidateSession())
		return;

	if (clientSession.disconnectFlag) {
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::RELEASE_SESSION);
		return;
	}

	// LAN, NET ����
	if (NetType::LAN == netType) {
		send_packet->SetLanHeader();
		send_packet->IncrementRefCount();
	}
	else {
		send_packet->SetNetHeader(protocolCode, privateKey);
		send_packet->IncrementRefCount();
	}

	clientSession.sendQ.Enqueue(send_packet);
	PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)&clientSession, (LPOVERLAPPED)PQCS_TYPE::SEND_POST);
}

// AsyncSend Call �õ�
bool NetClient::SendPost() {
	// Empty return
	if (clientSession.sendQ.GetUseCount() <= 0)
		return false;

	// Send 1ȸ üũ (send flag, true �� send ���� ��)
	if (clientSession.sendFlag == true)
		return false;
	if (InterlockedExchange8((char*)&clientSession.sendFlag, true) == true)
		return false;

	// Empty continue
	if (clientSession.sendQ.GetUseCount() <= 0) {
		InterlockedExchange8((char*)&clientSession.sendFlag, false);
		return SendPost();
	}

	AsyncSend();
	return true;
}

// WSASend() call
int NetClient::AsyncSend() {
	WSABUF wsaBuf[MAX_SEND_MSG];

	if (NetType::LAN == netType) {
		for (int i = 0; i < MAX_SEND_MSG; i++) {
			if (clientSession.sendQ.GetUseCount() <= 0) {
				clientSession.sendPacketCount = i;
				break;
			}
			clientSession.sendQ.Dequeue((PacketBuffer**)&clientSession.sendPacketArr[i]);
			wsaBuf[i].buf = clientSession.sendPacketArr[i]->GetLanPacketPos();
			wsaBuf[i].len = clientSession.sendPacketArr[i]->GetLanPacketSize();
		}
	}
	else {
		for (int i = 0; i < MAX_SEND_MSG; i++) {
			if (clientSession.sendQ.GetUseCount() <= 0) {
				clientSession.sendPacketCount = i;
				break;
			}
			clientSession.sendQ.Dequeue((PacketBuffer**)&clientSession.sendPacketArr[i]);
			wsaBuf[i].buf = clientSession.sendPacketArr[i]->GetNetPacketPos();
			wsaBuf[i].len = clientSession.sendPacketArr[i]->GetNetPacketSize();
		}
	}
	// MAX SEND ���� �ʰ�
	if (clientSession.sendPacketCount == 0) {
		clientSession.sendPacketCount = MAX_SEND_MSG;
		DisconnectSession();
		return false;
	}

	IncrementIOCount();
	ZeroMemory(&clientSession.sendOverlapped, sizeof(clientSession.sendOverlapped));
	if (SOCKET_ERROR == WSASend(clientSession.sock, wsaBuf, clientSession.sendPacketCount, NULL, 0, &clientSession.sendOverlapped, NULL)) {
		const auto err_no = WSAGetLastError();
		if (ERROR_IO_PENDING != err_no) { // Send ����
			////LOG("NetClient", LOG_LEVEL_DEBUG, "WSASend() Fail, Error code : %d", WSAGetLastError());
			DisconnectSession();
			DecrementIOCount();
			return false;
		}
	}
	return true;
}

bool NetClient::AsyncRecv() {
	DWORD flags = 0;
	WSABUF wsaBuf[2];

	// Recv Write Pos
	wsaBuf[0].buf = clientSession.recvBuf.GetWritePos();
	wsaBuf[0].len = clientSession.recvBuf.DirectEnqueueSize();
	// Recv Remain Pos
	wsaBuf[1].buf = clientSession.recvBuf.GetBeginPos();
	wsaBuf[1].len = clientSession.recvBuf.RemainEnqueueSize();

	IncrementIOCount();
	ZeroMemory(&clientSession.recvOverlapped, sizeof(clientSession.recvOverlapped));
	if (SOCKET_ERROR == WSARecv(clientSession.sock, wsaBuf, 2, NULL, &flags, &clientSession.recvOverlapped, NULL)) {
		if (WSAGetLastError() != ERROR_IO_PENDING) { // Recv ����
			////LOG("NetClient", LOG_LEVEL_DEBUG, "WSARecv() Fail, Error code : %d", WSAGetLastError());
			DecrementIOCount();
			return false;
		}
	}

	// Disconnect üũ
	if (clientSession.disconnectFlag) {
		CancelIoEx((HANDLE)clientSession.sock, NULL);
		return false;
	}
	return true;
}

void NetClient::RecvCompletionLAN(){
	// ��Ŷ ����
	for (;;) {
		int recv_len = clientSession.recvBuf.GetUseSize();
		if (recv_len <= LAN_HEADER_SIZE)
			break;

		LanHeader lanHeader;
		clientSession.recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		// ���̷ε� ������ ����
		if (recv_len < lanHeader.len + LAN_HEADER_SIZE)
			break;

		//------------------------------
		// OnRecv (��Ʈ��ũ ��� ����)
		//------------------------------
		PacketBuffer* csContentsPacket = PacketBuffer::Alloc();

		// ������ ��Ŷ ����
		clientSession.recvBuf.MoveFront(LAN_HEADER_SIZE);
		clientSession.recvBuf.Dequeue(csContentsPacket->writePos, lanHeader.len);
		csContentsPacket->MoveWp(lanHeader.len);

		// ����� ��Ŷ ó��
		OnRecv(csContentsPacket);
		InterlockedIncrement(&recvMsgCount);

		PacketBuffer::Free(csContentsPacket);
	}

	//------------------------------
	// Post Recv (Recv �ɾ�α�)
	//------------------------------
	if (false == clientSession.disconnectFlag) {
		AsyncRecv();
	}
}

void NetClient::RecvCompletionNET(){
	// ��Ŷ ����
	for (;;) {
		int recv_len = clientSession.recvBuf.GetUseSize();
		if (recv_len < NET_HEADER_SIZE)
			break;

		// ��� ī��
		char encryptPacket[200];
		clientSession.recvBuf.Peek(encryptPacket, NET_HEADER_SIZE);

		// code �˻�
		BYTE code = ((NetHeader*)encryptPacket)->code;
		if (code != protocolCode) {
			////LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong code!!", WSAGetLastError());
			DisconnectSession();
			break;
		}

		// ���̷ε� ������ ����
		WORD payload_len = ((NetHeader*)encryptPacket)->len;
		if (recv_len < (NET_HEADER_SIZE + payload_len)) {
			break;
		}

		// Recv Data ��Ŷ ȭ
		clientSession.recvBuf.MoveFront(NET_HEADER_SIZE);
		clientSession.recvBuf.Dequeue(encryptPacket + NET_HEADER_SIZE, payload_len);

		// ��ȣ��Ŷ ����
		PacketBuffer* decrypt_packet = PacketBuffer::Alloc();
		if (!decrypt_packet->DecryptPacket(encryptPacket, privateKey)) {
			PacketBuffer::Free(decrypt_packet);
			////LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong checksum!!", WSAGetLastError());
			DisconnectSession();
			break;
		}

		// ����� ��Ŷ ó��
		OnRecv(decrypt_packet);
		InterlockedIncrement(&recvMsgCount);

		// ��ȣ��Ŷ, ��ȣȭ ��Ŷ Free
		PacketBuffer::Free(decrypt_packet);
	}

	//------------------------------
	// Post Recv
	//------------------------------
	if (!clientSession.disconnectFlag) {
		AsyncRecv();
	}
}

bool NetClient::ValidateSession() {
	IncrementIOCount();

	// ���� ������ ����
	if (true == clientSession.releaseFlag) {
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
	// Connect Thread ����
	reconnectFlag = false;
	if (connectThread.joinable()) {
		connectThread.join();
	}

	// ���� ����
	if (!clientSession.releaseFlag) {
		DisconnectSession();
	}

	// ���� ���� üũ
	while (!clientSession.releaseFlag) {
		Sleep(100);
	}

	// Worker ����
	PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
	if (workerThread.joinable()) {
		workerThread.join();
	}

	CloseHandle(h_iocp);
	WSACleanup();

	// ����� ���ҽ� ����
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