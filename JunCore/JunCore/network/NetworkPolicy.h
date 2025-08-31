#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <vector>
#include <thread>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/container/LFStack.h"

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
		engine->CallSendPost(session);
	}
	
	template<typename NetworkEngineT>
	static constexpr void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		engine->CallReleaseSession(session);
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
// ClientPolicy - 멀티세션 지원 클라이언트 정책  
//------------------------------
struct ClientPolicy 
{
	// 정책 식별
	static constexpr bool IsServer = false;
	static constexpr const char* PolicyName = "Client";

	// 세션 관리 타입 정의
	using SessionContainer = std::vector<Session>;  // 멀티 세션 지원
	using SessionIndex = size_t;                    // 벡터 인덱스
	using SocketType = SOCKET;                      // client socket

	// 클라이언트 특화 멤버 변수들
	struct PolicyData 
	{
		// 네트워크
		char serverIP[16] = { 0, };
		WORD serverPort = 0;
		
		// 세션 관리 (멀티세션 지원)
		SessionContainer sessions;              // 세션 배열
		size_t maxSessions = 1;                // 최대 세션 수 (기본 1개, 더미용은 더 많이)
		size_t currentSessions = 0;            // 현재 활성 세션 수
		LFStack<size_t> availableIndexes;     // 사용 가능한 인덱스 스택
		DWORD sessionUnique = 0;               // 세션 고유 ID
		
		// 스레드 관리
		std::thread workerThread;
		std::thread connectThread;
		
		// 옵션
		bool reconnectFlag = true;
		bool isDummyClient = false;            // 더미 클라이언트 모드
		
		// 더미 클라이언트 전용
		DWORD connectInterval = 100;           // 연결 간격 (ms)
		
		// 클라이언트 특화 (Parser)
		// Parser는 NetworkEngine에서 별도 관리
		
		// 소멸자에서 리소스 정리
		~PolicyData() {
			// 세션들은 vector의 소멸자에서 자동 정리
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
		engine->CallSendPost(session);
	}
	
	template<typename NetworkEngineT>
	static constexpr void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session)
	{
		engine->CallReleaseSession(session);
	}
	
	template<typename NetworkEngineT>
	static constexpr void UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data)
	{
		// 클라이언트는 추가 TPS 없음 (NetBase에서 기본 TPS만 사용)
	}
	
	// 세션 관련 (멀티세션 지원)
	static SessionId GetSessionId(PolicyData& data);
	static Session* ValidateSession(PolicyData& data, SessionId sessionId);
	static bool ReleaseSession(PolicyData& data, Session* session);
	
	// 더미 클라이언트 전용
	static void ConnectAllSessions(PolicyData& data);
	static void DisconnectAllSessions(PolicyData& data);
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
	for (unsigned int i = 0; i < data.maxSession; i++)
	{
		data.sessionIdxStack.Push(data.maxSession - (1 + i));
	}

	// IOCP 초기화 (직접 NetworkEngine 사용 시 필요, NetworkManager 사용 시 덮어쓰기됨)
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
	// 워커 스레드 생성 (직접 NetworkEngine 사용 시 필요, NetworkManager 사용 시 중복되지만 무해함)
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

	// NetworkManager가 워커 스레드 종료를 담당
	// for (int i = 0; i < data.maxWorker; i++)
	// {
	//     PostQueuedCompletionStatus(engine->h_iocp, 0, 0, 0);
	// }

	// 스레드 종료 대기 (NetworkManager가 담당)
	// for (int i = 0; i < data.maxWorker; i++)
	// {
	//     if (data.workerThreadArr[i].joinable())
	//     {
	//         data.workerThreadArr[i].join();
	//     }
	// }

	if (data.acceptThread.joinable())
	{
		data.acceptThread.join();
	}

	if (data.timeOutThread.joinable())
	{
		data.timeOutThread.join();
	}

	engine->CallOnServerStop();
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
	data.maxSessions = 1;        // 기본 1개 세션
	data.isDummyClient = false;  // 기본 일반 클라이언트

	/* TODO: Parser 사용 구현 예정
	Parser parser;
	parser.LoadFile(systemFile);
	parser.GetValue(configSection, "IP", data.serverIP);
	parser.GetValue(configSection, "PORT", (int*)&data.serverPort);
	parser.GetValue(configSection, "MAX_SESSIONS", (int*)&data.maxSessions);
	parser.GetValue(configSection, "IS_DUMMY_CLIENT", (int*)&data.isDummyClient);
	parser.GetValue(configSection, "CONNECT_INTERVAL", (int*)&data.connectInterval);
	*/

	// 세션 벡터 초기화
	data.sessions.resize(data.maxSessions);
	
	// 사용 가능한 인덱스 스택 초기화
	for (size_t i = data.maxSessions; i > 0; --i) {
		data.availableIndexes.Push(i - 1);
	}

	// IOCP 초기화 (직접 NetworkEngine 사용 시 필요, NetworkManager 사용 시 덮어쓰기됨)
	engine->InitializeIOCP(1);

	// 모든 세션에 IOCP 핸들 설정 (NetworkManager가 설정한 공유 IOCP 사용)
	for (auto& session : data.sessions) {
		session.SetIOCP(engine->GetIOCPHandle());
	}
}

template<typename NetworkEngineT>
inline void ClientPolicy::StartNetwork(NetworkEngineT* engine, PolicyData& data)
{
	// NetworkManager가 워커 스레드를 관리하므로 개별 워커 스레드 생성 안 함
	// data.workerThread = std::thread([engine] { engine->WorkerFunc(); });
	
	// Connect 스레드 시작
	data.connectThread = std::thread([engine] { engine->ConnectFunc(); });
}

template<typename NetworkEngineT>
inline void ClientPolicy::StopNetwork(NetworkEngineT* engine, PolicyData& data)
{
	data.reconnectFlag = false;

	// 모든 세션 소켓 정리
	DisconnectAllSessions(data);

	// 워커 스레드 종료 (NetworkManager가 담당)
	// if (data.workerThread.joinable())
	// {
	//     data.workerThread.join();
	// }

	if (data.connectThread.joinable())
	{
		data.connectThread.join();
	}

	engine->CallOnClientStop();
}

inline SessionId ClientPolicy::GetSessionId(PolicyData& data)
{
	if (data.availableIndexes.GetUseCount() <= 0)
		return SessionId(INVALID_SESSION_ID);

	size_t index;
	data.availableIndexes.Pop(&index);
	return SessionId((DWORD)index, ++data.sessionUnique);
}

inline Session* ClientPolicy::ValidateSession(PolicyData& data, SessionId sessionId)
{
	if (sessionId.SESSION_INDEX >= data.sessions.size())
		return nullptr;
		
	Session* session = &data.sessions[sessionId.SESSION_INDEX];
	if (session->sessionId.sessionId != sessionId.sessionId)
		return nullptr;
	
	session->IncrementIOCount();
	return session;
}

inline bool ClientPolicy::ReleaseSession(PolicyData& data, Session* session)
{
	if (InterlockedCompareExchange((LONG*)&session->releaseFlag, TRUE, FALSE) == TRUE)
		return false;

	// 세션 인덱스 반환
	size_t index = session - &data.sessions[0];  // 포인터 연산으로 인덱스 계산
	data.availableIndexes.Push(index);
	InterlockedDecrement((LONG*)&data.currentSessions);
	
	closesocket(session->sock);
	session->sock = INVALID_SOCKET;
	session->releaseFlag = TRUE;
	
	return true;
}

inline void ClientPolicy::ConnectAllSessions(PolicyData& data)
{
	// 더미 클라이언트용: 모든 세션을 순차적으로 연결
	for (size_t i = 0; i < data.maxSessions; ++i)
	{
		if (!data.reconnectFlag) break;
		
		Session* session = &data.sessions[i];
		
		// 소켓 생성
		session->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (session->sock == INVALID_SOCKET)
		{
			Sleep(data.connectInterval);
			continue;
		}
		
		// 연결 시도
		SOCKADDR_IN serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(data.serverPort);
		serverAddr.sin_addr.s_addr = inet_addr(data.serverIP);
		
		if (connect(session->sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		{
			closesocket(session->sock);
			session->sock = INVALID_SOCKET;
			Sleep(data.connectInterval);
			continue;
		}
		
		// 세션 초기화
		SessionId newSessionId((DWORD)i, ++data.sessionUnique);
		session->Set(session->sock, serverAddr.sin_addr, data.serverPort, newSessionId);
		
		InterlockedIncrement((LONG*)&data.currentSessions);
		
		if (data.connectInterval > 0) {
			Sleep(data.connectInterval);  // 연결 간격
		}
	}
}

inline void ClientPolicy::DisconnectAllSessions(PolicyData& data)
{
	for (auto& session : data.sessions)
	{
		if (session.sock != INVALID_SOCKET)
		{
			closesocket(session.sock);
			session.sock = INVALID_SOCKET;
		}
	}
	data.currentSessions = 0;
}