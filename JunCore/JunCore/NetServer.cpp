﻿#include <WS2tcpip.h>
#include <WinSock2.h>
#include <memory.h>
#include <timeapi.h>
#include <atomic>
#include <iostream>
#include "NetServer.h"
#include "protocol.h"
#include "../JunCommon/lib/RingBuffer.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

//------------------------------
// Server Func
//------------------------------
NetServer::NetServer(const char* system_file, const char* server) 
{
	int serverPort;
	int nagle;

	// Read SystemFile
	parser.LoadFile(system_file);

	// json parser로 변경할것
	protocolCode	= 0xFF;			// parser.GetValue(server, "PROTOCOL_CODE", (int*)&protocolCode);
	privateKey		= 0xFF;			// parser.GetValue(server, "PRIVATE_KEY", (int*)&privateKey);
	netType			= NetType::NET; // parser.GetValue(server, "NET_TYPE", (int*)&netType);
	serverPort		= 7777;			// parser.GetValue(server, "PORT", (int*)&serverPort);
	maxSession		= 10000;		// parser.GetValue(server, "MAX_SESSION", (int*)&maxSession);
	nagle			= TRUE;			// parser.GetValue(server, "NAGLE", (int*)&nagle);
	timeoutFlag		= TRUE;			// parser.GetValue(server, "TIME_OUT_FLAG", (int*)&timeoutFlag);
	timeOut			= 60000/*1m*/;	// parser.GetValue(server, "TIME_OUT", (int*)&timeOut);
	timeoutCycle	= 10000/*10s*/; // parser.GetValue(server, "TIME_OUT_CYCLE", (int*)&timeoutCycle);
	maxWorker		= 4;			// parser.GetValue(server, "MAX_WORKER", (int*)&maxWorker);
	activeWorker	= 2;			// parser.GetValue(server, "ACTIVE_WORKER", (int*)&activeWorker);

	// Check system
	if (maxWorker < activeWorker) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "WORKER_NUM_ERROR");
		throw std::exception("WORKER_NUM_ERROR");
	} 

	if (1 < (BYTE)netType) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "NET_TYPE_ERROR");
		throw std::exception("NET_TYPE_ERROR");
	}

	//////////////////////////////
	// Set Server
	//////////////////////////////

	WSADATA wsaData;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "WSAStartup()_ERROR : %d", WSAGetLastError());
		throw std::exception("WSAStartup_ERROR");
	}

	listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == listenSock) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "WSASocket()_ERROR : %d", WSAGetLastError());
		throw std::exception("WSASocket_ERROR");
	}

	// Reset nagle
	if (!nagle) 
	{
		int opt_val = TRUE;
		if (setsockopt(listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt_val, sizeof(opt_val)) != 0) 
		{
			//LOG("NetServer", LOG_LEVEL_FATAL, "SET_NAGLE_ERROR : %d", WSAGetLastError());
			throw std::exception("SET_NAGLE_ERROR");
		}
	}

	// Set Linger
	LINGER linger = { 1, 0 };
	if (setsockopt(listenSock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof linger) != 0) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "SET_LINGER_ERROR : %d", WSAGetLastError());
		throw std::exception("SET_LINGER_ERROR");
	}

	// bind 
	SOCKADDR_IN	server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverPort);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (0 != bind(listenSock, (SOCKADDR*)&server_addr, sizeof(SOCKADDR_IN))) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "bind()_ERROR : %d", WSAGetLastError());
		throw std::exception("bind()_ERROR");
	}

	// Set Session
	sessionArray = new Session[maxSession];
	for (int i = 0; i < maxSession; i++)
	{
		sessionIdxStack.Push(maxSession - (1 + i));
	}

	// Create IOCP
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, activeWorker);
	if (h_iocp == INVALID_HANDLE_VALUE || h_iocp == NULL) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "CreateIoCompletionPort()_ERROR : %d", GetLastError());
		throw std::exception("CreateIoCompletionPort()_ERROR");
	}

	// Create IOCP Worker
	workerThreadArr = new thread[maxWorker];
	for (int i = 0; i < maxWorker; i++) 
	{
		workerThreadArr[i] = thread([this] {WorkerFunc(); });
	}
}

NetServer::~NetServer()
{
	delete[] sessionArray;
	delete[] workerThreadArr;
}

void NetServer::Start()
{
	// listen
	if (0 != listen(listenSock, SOMAXCONN_HINT(65535))) 
	{
		//LOG("NetServer", LOG_LEVEL_FATAL, "listen()_ERROR : %d", WSAGetLastError());
		throw std::exception("listen()_ERROR");
	}

	// Create Thread
	acceptThread = thread([this] {AcceptFunc(); });
	if (timeoutFlag) 
	{
		timeOutThread = thread([this] {TimeoutFunc(); });
	}

	// LOG
	//LOG("NetServer", LOG_LEVEL_INFO, "Start NetServer");
}

void NetServer::AcceptFunc() 
{
	for (;;) 
	{
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);

		//------------------------------
		// Accept
		//------------------------------
		auto acceptSock = accept(listenSock, (sockaddr*)&clientAddr, &addrLen);
		acceptTotal++;
		if (acceptSock == INVALID_SOCKET)
		{
			auto errNo = WSAGetLastError();
			// 정상 종료
			if (errNo == WSAEINTR || errNo == WSAENOTSOCK)
			{
				acceptTotal--;
				break;
			}
			// accept Error
			else
			{
				//LOG("NetServer", LOG_LEVEL_WARN, "accept() Fail, Error code : %d", errNo);
				continue;
			}
		}
		if (maxSession <= sessionCount)
		{
			closesocket(acceptSock);
		}

		//------------------------------
		// 접속 허용 판별
		//------------------------------
		in_addr acceptIp = clientAddr.sin_addr;
		WORD acceptPort = ntohs(clientAddr.sin_port);
		if (false == OnConnectionRequest(acceptIp, acceptPort))
	{
			closesocket(acceptSock);
			continue;
		}

		//------------------------------
		// 세션 할당
		//------------------------------

		// ID 할당
		SessionId sessionId = GetSessionId();
		if (sessionId == INVALID_SESSION_ID)
	{
			closesocket(acceptSock);
			continue;
		}

		// 세션 자료구조 할당
		Session* p_acceptSession = &sessionArray[sessionId.SESSION_INDEX];
		p_acceptSession->Set(acceptSock, acceptIp, acceptPort, sessionId);
		acceptCount++;

		// Bind IOCP
		CreateIoCompletionPort((HANDLE)acceptSock, h_iocp, (ULONG_PTR)p_acceptSession, 0);

		//------------------------------
		// 접속 로그인 후 작업
		//------------------------------
		OnClientJoin(sessionId.sessionId);
		InterlockedIncrement((LONG*)&sessionCount);

		//------------------------------
		// WSARecv
		//------------------------------
		AsyncRecv(p_acceptSession);

		// 접속 I/O Count 감소
		DecrementIOCount(p_acceptSession);
	}
}

void NetServer::TimeoutFunc() 
{
	for (;;) 
	{
		Sleep(timeoutCycle);
		INT64 cur_time = timeGetTime();
		for (int i = 0; i < maxSession; i++)
		{
			Session* session = &sessionArray[i];
			SessionId id = session->sessionId;

			// 타임아웃 처리 대상 아님 (Interlocked 연산 최소화하기 위함)
			if (sessionArray[i].releaseFlag || (timeOut > cur_time - sessionArray[i].lastRecvTime))
			{
				continue;
			}

			// 타임아웃 처리 과정에 IO 증가
			IncrementIOCount(session);
			// 이미 해제되어 있는지 판별.
			if (true == session->releaseFlag)
			{
				DecrementIOCount(session);
				continue;
			}
			// 타임아웃 시간 재판별.
			if (timeOut > cur_time - sessionArray[i].lastRecvTime)
			{
				DecrementIOCount(session);
				continue;
			}

			// 타임아웃 처리
			DisconnectSession(session);
			DecrementIOCount(session);
		}
	}
}

void NetServer::WorkerFunc() 
{
	for (;;)
	{
		DWORD ioSize = 0;
		Session* session = 0;
		LPOVERLAPPED p_overlapped = 0;

		BOOL retGQCS = GetQueuedCompletionStatus(h_iocp, &ioSize, (PULONG_PTR)&session, &p_overlapped, INFINITE);

		// 워커 스레드 종료
		if (ioSize == 0 && session == 0 && p_overlapped == 0)
		{
			PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
			return;
		}
		// FIN
		if (ioSize == 0)
		{
			if (&session->sendOverlapped == p_overlapped)
			{
				//LOG("NetServer", LOG_LEVEL_FATAL, "Zero Byte Send !!");
				goto Decrement_IOCount;
			}
		}
		// PQCS
		else if ((ULONG_PTR)p_overlapped < (ULONG_PTR)PQCS_TYPE::NONE)
		{
			switch ((PQCS_TYPE)(unsigned char)p_overlapped)
			{
			case PQCS_TYPE::SEND_POST:
			{
				SendPost(session);
				goto Decrement_IOCount;
			}

			case PQCS_TYPE::RELEASE_SESSION:
			{
				ReleaseSession(session);
				continue;
			}

			default:
				//LOG("NetServer", LOG_LEVEL_FATAL, "PQCS Default");
				break;
			}
		}

		// recv 완료처리
		if (&session->recvOverlapped == p_overlapped)
		{
			if (retGQCS)
			{
				session->recvBuf.MoveRear(ioSize);
				session->lastRecvTime = timeGetTime();
				// 패킷 타입 내부 외부 분기 처리
				if (NetType::LAN == netType)
				{
					RecvCompletionLan(session);
				}
				else {
					RecvCompletionNet(session);
				}
			}
			else
			{
				//LOG("NetServer", LOG_LEVEL_DEBUG, "Overlapped Recv Fail");
			}
		}
		// send 완료처리
		else if (&session->sendOverlapped == p_overlapped)
		{
			if (retGQCS)
			{
				SendCompletion(session);
			}
			else
			{
				//LOG("NetServer", LOG_LEVEL_DEBUG, "Overlapped Send Fail");
			}
		}
		else 
		{
			//LOG("NetServer", LOG_LEVEL_FATAL, "GQCS INVALID Overlapped!!");
		}

	Decrement_IOCount:
		DecrementIOCount(session);
	}
}

bool NetServer::ReleaseSession(Session* session) 
{
	if (0 != session->ioCount)
	{
		return false;
	}

	// * releaseFlag(0), iocount(0) -> releaseFlag(1), iocount(0)
	if (0 == InterlockedCompareExchange64((long long*)&session->releaseFlag, 1, 0)) 
	{
		// 리소스 정리 (소켓, 패킷)
		closesocket(session->sock);
		PacketBuffer* packet;

		while (session->sendQ.Dequeue(&packet))
		{
			PacketBuffer::Free(packet);
		}

		for (int i = 0; i < session->sendPacketCount; i++)
		{
			PacketBuffer::Free(session->sendPacketArr[i]);
		}

		// 컨텐츠 리소스 정리
		OnClientLeave(session->sessionId);

		// 세션 반환
		sessionIdxStack.Push(session->sessionId.SESSION_INDEX);
		InterlockedDecrement((LONG*)&sessionCount);
		return true;
	}
	return false;
}

void NetServer::SendCompletion(Session* session)
{
	// Send Packet Free
	for (int i = 0; i < session->sendPacketCount ; i++)
	{
		PacketBuffer::Free(session->sendPacketArr[i]);
	}

	InterlockedAdd((LONG*)&sendMsgCount, session->sendPacketCount);
	session->sendPacketCount = 0;

	// Send Flag OFF
	InterlockedExchange8((char*)&session->sendFlag, false);

	// Send 재전송 체크
	if (session->disconnectFlag || session->sendQ.GetUseCount() <= 0)
	{
		return;
	}

	SendPost(session);
}

// SendQ Enqueue, SendPost
void NetServer::SendPacket(SessionId session_id, PacketBuffer* send_packet)
{
	Session* session = ValidateSession(session_id);
	if (nullptr == session)
	{
		return;
	}

	if (session->disconnectFlag)
	{
		DecrementIOCountPQCS(session);
		return;
	}

	// LAN, NET 구분
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

	session->sendQ.Enqueue(send_packet);
#if PQCS_SEND
	if (session->sendFlag == false)
	{
		PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)session, (LPOVERLAPPED)PQCS_TYPE::SEND_POST);
	}
	else
	{
		DecrementIOCountPQCS(session);
	}
#else
	AsyncSend(session);
	DecrementIOCountPQCS(session);
#endif
}

// AsyncSend Call �õ�
bool NetServer::SendPost(Session* session)
{
	// Empty return
	if (session->sendQ.GetUseCount() <= 0)
		return false;

	// Send 1회 체크 (send flag, true 면 send 진행 중)
	if (session->sendFlag == true)
		return false;
	if (InterlockedExchange8((char*)&session->sendFlag, true) == true)
		return false;

	// Empty continue
	if (session->sendQ.GetUseCount() <= 0)
	{
		InterlockedExchange8((char*)&session->sendFlag, false);
		return SendPost(session);
	}

	AsyncSend(session);
	return true;
}

// WSASend() call
int NetServer::AsyncSend(Session* session)
{
	WSABUF wsaBuf[MAX_SEND_MSG];

	// MAX SEND 개수 초과
	if (MAX_SEND_MSG < session->sendQ.GetUseCount())
	{
		DisconnectSession(session);
		return false;
	}
	if (NetType::LAN == netType)
	{
		for (int i = 0; (i < MAX_SEND_MSG && 0 < session->sendQ.GetUseCount()); i++)
		{
			session->sendQ.Dequeue((PacketBuffer**)&session->sendPacketArr[i]);
			session->sendPacketCount++;
			wsaBuf[i].buf = session->sendPacketArr[i]->GetLanPacketPos();
			wsaBuf[i].len = session->sendPacketArr[i]->GetLanPacketSize();
		}
	}
	else
	{
		for (int i = 0; (i < MAX_SEND_MSG && 0 < session->sendQ.GetUseCount()); i++)
		{
			session->sendQ.Dequeue((PacketBuffer**)&session->sendPacketArr[i]);
			session->sendPacketCount++;
			wsaBuf[i].buf = session->sendPacketArr[i]->GetNetPacketPos();
			wsaBuf[i].len = session->sendPacketArr[i]->GetNetPacketSize();
		}
	}

	IncrementIOCount(session);
	ZeroMemory(&session->sendOverlapped, sizeof(session->sendOverlapped));
	if (SOCKET_ERROR == WSASend(session->sock, wsaBuf, session->sendPacketCount , NULL, 0, &session->sendOverlapped, NULL))
	{
		const auto err_no = WSAGetLastError();
		if (ERROR_IO_PENDING != err_no) // Send 실패
		{
			//LOG("NetServer", LOG_LEVEL_DEBUG, "WSASend() Fail, Error code : %d", WSAGetLastError());
			DisconnectSession(session);
			DecrementIOCount(session);
			return false;
		}
	}
	return true;
}

bool NetServer::AsyncRecv(Session* session)
{
	DWORD flags = 0;
	WSABUF wsaBuf[2];

	// Recv Write Pos
	wsaBuf[0].buf = session->recvBuf.GetWritePos();
	wsaBuf[0].len = session->recvBuf.DirectEnqueueSize();
	// Recv Remain Pos
	wsaBuf[1].buf = session->recvBuf.GetBeginPos();
	wsaBuf[1].len = session->recvBuf.RemainEnqueueSize();

	IncrementIOCount(session);
	ZeroMemory(&session->recvOverlapped, sizeof(session->recvOverlapped));
	if (SOCKET_ERROR == WSARecv(session->sock, wsaBuf, 2, NULL, &flags, &session->recvOverlapped, NULL))
	{
		if (WSAGetLastError() != ERROR_IO_PENDING) // Recv 실패
		{
			//LOG("NetServer", LOG_LEVEL_DEBUG, "WSARecv() Fail, Error code : %d", WSAGetLastError());
			DecrementIOCount(session);
			return false;
		}
	}

	// Disconnect 체크
	if (session->disconnectFlag)
	{
		CancelIoEx((HANDLE)session->sock, NULL);
		return false;
	}
	return true;
}

void NetServer::RecvCompletionLan(Session* session)
{
	// 패킷 처리
	for (;;)
	{
		int recvLen = session->recvBuf.GetUseSize();
		if (recvLen <= LAN_HEADER_SIZE)
			break;

		LanHeader lanHeader;
		session->recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		// 페이로드 길이만큼 수신
		if (recvLen < lanHeader.len + LAN_HEADER_SIZE)
			break;

		//------------------------------
		// OnRecv (네트워크 분기 처리)
		//------------------------------
		PacketBuffer* contentsPacket = PacketBuffer::Alloc();

		// 수신한 패킷 분해
		session->recvBuf.MoveFront(LAN_HEADER_SIZE);
		session->recvBuf.Dequeue(contentsPacket->writePos, lanHeader.len);
		contentsPacket->MoveWp(lanHeader.len);

		// 컨텐츠 패킷 처리
		OnRecv(session->sessionId, contentsPacket);
		InterlockedIncrement(&recvMsgCount);

		PacketBuffer::Free(contentsPacket);
	}

	//------------------------------
	// Post Recv (Recv 재등록)
	//------------------------------
	if (false == session->disconnectFlag)
	{
		AsyncRecv(session);
	}
}

void NetServer::RecvCompletionNet(Session* session)
{
	// 패킷 처리
	for (;;)
	{
		int recvLen = session->recvBuf.GetUseSize();
		if (recvLen <= NET_HEADER_SIZE)
		{
			break;
		}

		// 암호 카피
		char encryptPacket[NET_HEADER_SIZE + MAX_PAYLOAD_LEN];
		session->recvBuf.Peek(encryptPacket, NET_HEADER_SIZE);

		// code 검사
		BYTE code = ((NetHeader*)encryptPacket)->code;
		if (code != protocolCode)
		{
			//LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong code!!", WSAGetLastError());
			DisconnectSession(session);
			break;
		}

		// 페이로드 길이만큼 수신
		WORD payloadLen = ((NetHeader*)encryptPacket)->len;
		if (recvLen < (NET_HEADER_SIZE + payloadLen))
		{
			break;
		}

		// 암호패킷 분해
		session->recvBuf.MoveFront(NET_HEADER_SIZE);
		session->recvBuf.Dequeue(encryptPacket + NET_HEADER_SIZE, payloadLen);

		// 패킷 복호화
		PacketBuffer* decrypt_packet = PacketBuffer::Alloc();
		if (!decrypt_packet->DecryptPacket(encryptPacket, privateKey))
		{
			PacketBuffer::Free(decrypt_packet);
			//LOG("NetServer", LOG_LEVEL_WARN, "Recv Packet is wrong checksum!!", WSAGetLastError());
			DisconnectSession(session);
			break;
		}

		// 컨텐츠 패킷 처리
		OnRecv(session->sessionId, decrypt_packet);
		InterlockedIncrement(&recvMsgCount);

		// 암호패킷, 복호화 패킷 Free
		PacketBuffer::Free(decrypt_packet);
	}

	//------------------------------
	// Post Recv
	//------------------------------
	if (!session->disconnectFlag)
	{
		AsyncRecv(session);
	}
}

Session* NetServer::ValidateSession(SessionId session_id)
{
	Session* session = &sessionArray[session_id.SESSION_INDEX];
	IncrementIOCount(session);

	// 세션 해제됨 판별
	if (true == session->releaseFlag)
	{
		DecrementIOCountPQCS(session);
		return nullptr;
	}
	// 세션 일치
	if (session->sessionId != session_id)
	{
		DecrementIOCountPQCS(session);
		return nullptr;
	}

	// 유효 검증세션 반환
	return session;
}

void NetServer::Disconnect(SessionId session_id)
{
	Session* session = ValidateSession(session_id);
	if (nullptr == session) return;
	DisconnectSession(session);
	DecrementIOCountPQCS(session);
}

void NetServer::Stop()
{
	// AcceptThread 종료
	closesocket(listenSock);
	if (acceptThread.joinable())
	{
		acceptThread.join();
	}

	// 세션 종료
	for (int i = 0; i < maxSession; i++)
	{
		Session* session = &sessionArray[i];
		if (true == session->releaseFlag)
			continue;
		DisconnectSession(session);
	}

	// 세션 종료 체크
	for (;;)
	{
		if (sessionCount == 0)
		{
			break;
		}
		Sleep(10);
	}

	// Worker 종료
	PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
	for (int i = 0; i < maxWorker; i++)
	{
		if (workerThreadArr[i].joinable())
		{
			workerThreadArr[i].join();
		}
	}

	CloseHandle(h_iocp);
	WSACleanup();

	// 컨텐츠 서버 리소스 정리
	OnServerStop();
}

//------------------------------
// SessionId
//------------------------------

SessionId NetServer::GetSessionId()
{
	DWORD index;
	if (false == sessionIdxStack.Pop(&index))
	{
		return INVALID_SESSION_ID;
	}

	SessionId sessionId(index, ++sessionUnique);
	return sessionId;
}