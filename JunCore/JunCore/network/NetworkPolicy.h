#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <vector>
#include <thread>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/container/LFStack.h"

//------------------------------
// Policy-Based Design for NetworkEngine
// Server와 Client의 동작 차이를 정책으로 분리
//------------------------------

//------------------------------
// ServerPolicy - 서버 특화 정책
//------------------------------
struct ServerPolicy 
{
	// 정책 식별
	static constexpr bool IsServer = true;
	static constexpr const char* PolicyName = "Server";

	// 세션 관리 타입 정의
	using SessionContainer = Session*;  // 세션 배열 포인터
	using SessionIndex = DWORD;
	using SocketType = SOCKET;  // listen socket

	// 서버 특화 멤버 변수들
	struct PolicyData 
	{
		// 네트워크
		SOCKET listenSock = INVALID_SOCKET;
		
		// 세션 관리
		DWORD sessionUnique = 0;
		SessionContainer sessionArray = nullptr;
		DWORD maxSession = 0;
		LFStack<DWORD> sessionIdxStack;
		
		// 스레드 관리
		WORD maxWorker = 0;
		WORD activeWorker = 0;
		std::thread* workerThreadArr = nullptr;
		std::thread acceptThread;
		std::thread timeOutThread;
		
		// 옵션
		bool timeoutFlag = false;
		DWORD timeoutCycle = 0;
		DWORD timeOut = 0;
		
		// 서버 특화 모니터링
		DWORD acceptCount = 0;
		DWORD acceptTPS = 0;
		DWORD acceptTotal = 0;
		alignas(64) DWORD sessionCount = 0;
		
		// 소멸자에서 리소스 정리
		~PolicyData() {
			if (sessionArray) delete[] sessionArray;
			if (workerThreadArr) delete[] workerThreadArr;
		}
	};

	// Policy-specific operations
	template<typename NetworkEngineT>
	static void Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection);
	
	template<typename NetworkEngineT>
	static void StartNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void StopNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static constexpr void HandleSendPost(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		engine->SendPost(session);
	}
	
	template<typename NetworkEngineT>
	static constexpr void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		engine->ReleaseSession(session);
	}
	
	template<typename NetworkEngineT>
	static constexpr void UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data)
	{
		data.acceptTPS = data.acceptCount;
		data.acceptCount = 0;
	}
	
	// 세션 관련
	static SessionId GetSessionId(PolicyData& data);
	static Session* ValidateSession(PolicyData& data, SessionId sessionId);
	static bool ReleaseSession(PolicyData& data, Session* session);
};

//------------------------------
// ClientPolicy - 클라이언트 특화 정책  
//------------------------------
struct ClientPolicy 
{
	// 정책 식별
	static constexpr bool IsServer = false;
	static constexpr const char* PolicyName = "Client";

	// 세션 관리 타입 정의
	using SessionContainer = Session;   // 단일 세션 객체
	using SessionIndex = void;          // 인덱스 불필요
	using SocketType = SOCKET;          // client socket

	// 클라이언트 특화 멤버 변수들
	struct PolicyData 
	{
		// 네트워크
		SOCKET clientSock = INVALID_SOCKET;
		char serverIP[16] = { 0, };
		WORD serverPort = 0;
		
		// 세션 (단일)
		SessionContainer clientSession;
		
		// 스레드 관리
		std::thread workerThread;
		std::thread connectThread;
		
		// 옵션
		bool reconnectFlag = true;
		
		// 클라이언트 특화 (Parser)
		// Parser는 NetworkEngine에서 별도 관리
	};

	// Policy-specific operations
	template<typename NetworkEngineT>
	static void Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection);
	
	template<typename NetworkEngineT>
	static void StartNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void StopNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static constexpr void HandleSendPost(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		// 클라이언트는 단일 세션이므로 session 파라미터 무시하고 자신의 세션 사용
		engine->SendPost(&data.clientSession);
	}
	
	template<typename NetworkEngineT>
	static constexpr void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		// 클라이언트는 자신의 세션만 해제
		engine->ReleaseSession(&data.clientSession);
	}
	
	template<typename NetworkEngineT>
	static constexpr void UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data)
	{
		// 클라이언트는 추가 TPS 없음 (NetBase에서 기본 TPS만 사용)
	}
	
	// 세션 관련 (클라이언트는 단순)
	static SessionId GetSessionId(PolicyData& data)
	{
		return SessionId(0, 1); // 클라이언트는 고정된 세션 ID
	}
	
	static Session* ValidateSession(PolicyData& data)
	{
		data.clientSession.IncrementIOCount();
		return &data.clientSession;
	}
	
	static bool ReleaseSession(PolicyData& data);
};

//==============================================
// Template implementation - header only
//==============================================

#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// ServerPolicy 템플릿 구현
//------------------------------

template<typename NetworkEngineT>
inline void ServerPolicy::Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection)
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
inline void ServerPolicy::StartNetwork(NetworkEngineT* engine, PolicyData& data)
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
inline void ServerPolicy::StopNetwork(NetworkEngineT* engine, PolicyData& data)
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

inline SessionId ServerPolicy::GetSessionId(PolicyData& data)
{
	if (data.sessionIdxStack.GetUseCount() <= 0)
		return SessionId(INVALID_SESSION_ID);

	DWORD index;
	data.sessionIdxStack.Pop(&index);
	return SessionId(index, ++data.sessionUnique);
}

inline Session* ServerPolicy::ValidateSession(PolicyData& data, SessionId sessionId)
{
	Session* session = &data.sessionArray[sessionId.SESSION_INDEX];
	if (session->sessionId.sessionId != sessionId.sessionId)
		return nullptr;
	
	session->IncrementIOCount();
	return session;
}

inline bool ServerPolicy::ReleaseSession(PolicyData& data, Session* session)
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
// ClientPolicy 템플릿 구현
//------------------------------

template<typename NetworkEngineT>
inline void ClientPolicy::Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection)
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
inline void ClientPolicy::StartNetwork(NetworkEngineT* engine, PolicyData& data)
{
	// 워커 스레드 시작
	data.workerThread = std::thread([engine] { engine->WorkerFunc(); });
	
	// Connect 스레드 시작
	data.connectThread = std::thread([engine] { engine->ConnectFunc(); });
}

template<typename NetworkEngineT>
inline void ClientPolicy::StopNetwork(NetworkEngineT* engine, PolicyData& data)
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

inline bool ClientPolicy::ReleaseSession(PolicyData& data)
{
	if (InterlockedCompareExchange((LONG*)&data.clientSession.releaseFlag, TRUE, FALSE) == TRUE)
		return false;

	closesocket(data.clientSession.sock);
	data.clientSession.sock = INVALID_SOCKET;
	data.clientSession.releaseFlag = TRUE;
	
	return true;
}