#include <WS2tcpip.h>
#include <WinSock2.h>
#include <memory.h>
#include <timeapi.h>
#include <atomic>
#include <iostream>
#include "server.h"
#include "IoCommon.h"
#include "../protocol/message.h"
#include "../../JunCommon/container/RingBuffer.h"
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
		p_acceptSession->SetIOCP(h_iocp);  // IOCP 핸들 설정
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
	ServerSessionManager sessionManager(this);
	IoCommon::IOCPWorkerLoop(h_iocp, sessionManager);
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
	IoCommon::SendCompletion(*session, 
		[this, session]() { SendPost(session); },
		[this](int count) { InterlockedAdd((LONG*)&sendMsgCount, count); }
	);
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

// AsyncSend Call 시도
bool NetServer::SendPost(Session* session)
{
	// Empty return
	if (session->sendQ.GetUseCount() <= 0)
	{
		return false;
	}

	// Send 1회 체크 (send flag, true 면 send 진행 중)
	if (session->sendFlag == true)
	{
		return false;
	}

	if (InterlockedExchange8((char*)&session->sendFlag, true) == true)
	{
		return false;
	}

	// 다른 스레드에서 SendQ를 비웠을 수 있으므로 SendQ의 Size를 더블체크한다.
	if (session->sendQ.GetUseCount() <= 0)
	{
		InterlockedExchange8((char*)&session->sendFlag, false);

		// 플래그를 휙득한 사이 다른 스레드에서 SendQ를 채웠을 수 있으므로 다시 Send 시도해본다.
		return SendPost(session);
	}

	// 송신 시 마다 send_seq_number 증가
	++session->send_seq_number_;

	// 패킷 송신
	AsyncSend(session);
	return true;
}

// WSASend() call
int NetServer::AsyncSend(Session* session)
{
	const bool isLan = (NetType::LAN == netType);
	auto disconnect = [this](Session* s) { DisconnectSession(s); };
	auto decIo = [this](Session* s) { DecrementIOCount(s); };
	return IoCommon::AsyncSend(*session, isLan, disconnect, decIo) ? 1 : 0;
}

bool NetServer::AsyncRecv(Session* session)
{
	auto decIo = [this](Session* s) { DecrementIOCount(s); };
	return IoCommon::AsyncRecv(*session, decIo);
}

void NetServer::RecvCompletionLan(Session* session)
{
	IoCommon::RecvCompletionLan(*session,
		[this, session](PacketBuffer* packet) { 
			OnRecv(session->sessionId, packet); 
			PacketBuffer::Free(packet);
		},
		[this, session]() { DisconnectSession(session); },
		[this, session]() { AsyncRecv(session); },
		[this]() { InterlockedIncrement(&recvMsgCount); }
	);
}

void NetServer::RecvCompletionNet(Session* session)
{
	IoCommon::RecvCompletionNet(*session, protocolCode, privateKey,
		[this, session](PacketBuffer* packet) { 
			OnRecv(session->sessionId, packet); 
			PacketBuffer::Free(packet);
		},
		[this, session]() { DisconnectSession(session); },
		[this, session]() { AsyncRecv(session); },
		[this]() { InterlockedIncrement(&recvMsgCount); }
	);
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