#include "NetworkPolicy.h"
#include "NetworkEngine.h"
#include <iostream>

//------------------------------
// ServerPolicy 구현
//------------------------------

template<typename NetworkEngineT>
void ServerPolicy::Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection)
{
	// 서버 설정 로딩 (현재는 하드코딩)
	int serverPort = 7777;
	data.maxSession = 10000;
	int nagle = TRUE;
	data.timeoutFlag = TRUE;
	data.timeOut = 60000;        // 1분
	data.timeoutCycle = 10000;   // 10초
	data.maxWorker = 4;
	data.activeWorker = 2;

	/* TODO: Parser 사용 구현 예정
	Parser parser;
	parser.LoadFile(systemFile);
	parser.GetValue(configSection, "PORT", (int*)&serverPort);
	parser.GetValue(configSection, "MAX_SESSION", (int*)&data.maxSession);
	parser.GetValue(configSection, "NAGLE", (int*)&nagle);
	parser.GetValue(configSection, "TIME_OUT_FLAG", (int*)&data.timeoutFlag);
	parser.GetValue(configSection, "TIME_OUT", (int*)&data.timeOut);
	parser.GetValue(configSection, "TIME_OUT_CYCLE", (int*)&data.timeoutCycle);
	parser.GetValue(configSection, "MAX_WORKER", (int*)&data.maxWorker);
	parser.GetValue(configSection, "ACTIVE_WORKER", (int*)&data.activeWorker);
	*/

	// 시스템 검증
	if (data.maxWorker < data.activeWorker) 
	{
		throw std::exception("WORKER_NUM_ERROR");
	}

	// Listen Socket 생성
	data.listenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == data.listenSock) 
	{
		throw std::exception("WSASocket_ERROR");
	}

	// Nagle 설정
	if (!nagle) 
	{
		int opt_val = TRUE;
		if (setsockopt(data.listenSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt_val, sizeof(opt_val)) != 0) 
		{
			throw std::exception("SET_NAGLE_ERROR");
		}
	}

	// Linger 설정
	LINGER linger = { 1, 0 };
	if (setsockopt(data.listenSock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof linger) != 0) 
	{
		throw std::exception("SET_LINGER_ERROR");
	}

	// bind 
	SOCKADDR_IN	server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverPort);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (0 != bind(data.listenSock, (SOCKADDR*)&server_addr, sizeof(SOCKADDR_IN))) 
	{
		throw std::exception("bind()_ERROR");
	}

	// 세션 배열 생성
	data.sessionArray = new Session[data.maxSession];
	for (int i = 0; i < data.maxSession; i++)
	{
		data.sessionIdxStack.Push(data.maxSession - (1 + i));
	}

	// IOCP 초기화
	engine->InitializeIOCP(data.activeWorker);

	// listen
	if (0 != listen(data.listenSock, SOMAXCONN)) 
	{
		throw std::exception("listen()_ERROR");
	}

	// 워커 스레드 배열 생성
	data.workerThreadArr = new std::thread[data.maxWorker];
}

template<typename NetworkEngineT>
void ServerPolicy::StartNetwork(NetworkEngineT* engine, PolicyData& data)
{
	// 워커 스레드 시작
	for (int i = 0; i < data.maxWorker; i++)
	{
		data.workerThreadArr[i] = std::thread([engine] { engine->WorkerFunc(); });
	}

	// Accept 스레드 시작
	data.acceptThread = std::thread([engine] { engine->AcceptFunc(); });

	// Timeout 스레드 시작 (옵션)
	if (data.timeoutFlag)
	{
		data.timeOutThread = std::thread([engine] { engine->TimeoutFunc(); });
	}
}

template<typename NetworkEngineT>
void ServerPolicy::StopNetwork(NetworkEngineT* engine, PolicyData& data)
{
	// Accept 소켓 정리
	if (data.listenSock != INVALID_SOCKET)
	{
		closesocket(data.listenSock);
		data.listenSock = INVALID_SOCKET;
	}

	// 모든 워커 스레드에 종료 신호 전송
	for (int i = 0; i < data.maxWorker; i++)
	{
		PostQueuedCompletionStatus(engine->h_iocp, 0, 0, 0);
	}

	// 스레드 종료 대기
	for (int i = 0; i < data.maxWorker; i++)
	{
		if (data.workerThreadArr[i].joinable())
		{
			data.workerThreadArr[i].join();
		}
	}

	if (data.acceptThread.joinable())
	{
		data.acceptThread.join();
	}

	if (data.timeOutThread.joinable())
	{
		data.timeOutThread.join();
	}

	engine->OnServerStop();
}

template<typename NetworkEngineT>
void ServerPolicy::UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data)
{
	// 서버 특화 TPS 업데이트
	data.acceptTPS = data.acceptCount;
	data.acceptCount = 0;
}

SessionId ServerPolicy::GetSessionId(PolicyData& data)
{
	if (data.sessionIdxStack.GetUseCount() <= 0)
		return SessionId(INVALID_SESSION_ID);

	DWORD index;
	data.sessionIdxStack.Pop(&index);
	return SessionId(index, ++data.sessionUnique);
}

Session* ServerPolicy::ValidateSession(PolicyData& data, SessionId sessionId)
{
	Session* session = &data.sessionArray[sessionId.SESSION_INDEX];
	if (session->sessionId.sessionId != sessionId.sessionId)
		return nullptr;
	
	session->IncrementIOCount();
	return session;
}

bool ServerPolicy::ReleaseSession(PolicyData& data, Session* session)
{
	if (InterlockedCompareExchange((LONG*)&session->releaseFlag, TRUE, FALSE) == TRUE)
		return false;

	// 세션 인덱스 반환
	data.sessionIdxStack.Push(session->sessionId.SESSION_INDEX);
	InterlockedDecrement((LONG*)&data.sessionCount);
	
	closesocket(session->sock);
	session->sock = INVALID_SOCKET;
	session->releaseFlag = TRUE;
	
	return true;
}

//------------------------------
// ClientPolicy 구현
//------------------------------

template<typename NetworkEngineT>
void ClientPolicy::Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection)
{
	// 클라이언트 설정 로딩 (현재는 하드코딩)
	strcpy_s(data.serverIP, "127.0.0.1");
	data.serverPort = 7777;

	/* TODO: Parser 사용 구현 예정
	Parser parser;
	parser.LoadFile(systemFile);
	parser.GetValue(configSection, "IP", data.serverIP);
	parser.GetValue(configSection, "PORT", (int*)&data.serverPort);
	*/

	// IOCP 초기화 (클라이언트는 워커 스레드 1개)
	engine->InitializeIOCP(1);

	// 클라이언트 세션에 IOCP 핸들 설정
	data.clientSession.SetIOCP(engine->h_iocp);
}

template<typename NetworkEngineT>
void ClientPolicy::StartNetwork(NetworkEngineT* engine, PolicyData& data)
{
	// 워커 스레드 시작
	data.workerThread = std::thread([engine] { engine->WorkerFunc(); });
	
	// Connect 스레드 시작
	data.connectThread = std::thread([engine] { engine->ConnectFunc(); });
}

template<typename NetworkEngineT>
void ClientPolicy::StopNetwork(NetworkEngineT* engine, PolicyData& data)
{
	data.reconnectFlag = false;

	// 클라이언트 소켓 정리
	if (data.clientSock != INVALID_SOCKET)
	{
		closesocket(data.clientSock);
		data.clientSock = INVALID_SOCKET;
	}

	// 워커 스레드 종료
	PostQueuedCompletionStatus(engine->h_iocp, 0, 0, 0);

	// 스레드 종료 대기
	if (data.workerThread.joinable())
	{
		data.workerThread.join();
	}

	if (data.connectThread.joinable())
	{
		data.connectThread.join();
	}

	engine->OnClientStop();
}

template<typename NetworkEngineT>
void ClientPolicy::UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data)
{
	// 클라이언트는 추가 TPS 없음
}

SessionId ClientPolicy::GetSessionId(PolicyData& data)
{
	return SessionId(0, 1); // 클라이언트는 고정된 세션 ID
}

Session* ClientPolicy::ValidateSession(PolicyData& data)
{
	data.clientSession.IncrementIOCount();
	return &data.clientSession;
}

bool ClientPolicy::ReleaseSession(PolicyData& data)
{
	if (InterlockedCompareExchange((LONG*)&data.clientSession.releaseFlag, TRUE, FALSE) == TRUE)
		return false;

	closesocket(data.clientSession.sock);
	data.clientSession.sock = INVALID_SOCKET;
	data.clientSession.releaseFlag = TRUE;
	
	return true;
}

// 템플릿 인스턴스화 (링커 에러 방지)
// 실제 사용될 NetworkEngine 타입들을 명시적으로 인스턴스화
class NetworkEngineServer;  // forward declaration
class NetworkEngineClient;  // forward declaration

// ServerPolicy 인스턴스화
template void ServerPolicy::Initialize(NetworkEngineServer*, PolicyData&, const char*);
template void ServerPolicy::StartNetwork(NetworkEngineServer*, PolicyData&);
template void ServerPolicy::StopNetwork(NetworkEngineServer*, PolicyData&);
template void ServerPolicy::UpdatePolicyTPS(NetworkEngineServer*, PolicyData&);

// ClientPolicy 인스턴스화  
template void ClientPolicy::Initialize(NetworkEngineClient*, PolicyData&, const char*);
template void ClientPolicy::StartNetwork(NetworkEngineClient*, PolicyData&);
template void ClientPolicy::StopNetwork(NetworkEngineClient*, PolicyData&);
template void ClientPolicy::UpdatePolicyTPS(NetworkEngineClient*, PolicyData&);