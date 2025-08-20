#pragma once
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

	// 정책별 특화 동작들
	template<typename NetworkEngineT>
	static void Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection);
	
	template<typename NetworkEngineT>
	static void StartNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void StopNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void HandleSendPost(NetworkEngineT* engine, PolicyData& data, Session* session);
	
	template<typename NetworkEngineT>
	static void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session);
	
	template<typename NetworkEngineT>
	static void UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data);
	
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

	// 정책별 특화 동작들
	template<typename NetworkEngineT>
	static void Initialize(NetworkEngineT* engine, PolicyData& data, const char* configSection);
	
	template<typename NetworkEngineT>
	static void StartNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void StopNetwork(NetworkEngineT* engine, PolicyData& data);
	
	template<typename NetworkEngineT>
	static void HandleSendPost(NetworkEngineT* engine, PolicyData& data, Session* session);
	
	template<typename NetworkEngineT>
	static void HandleReleaseSession(NetworkEngineT* engine, PolicyData& data, Session* session);
	
	template<typename NetworkEngineT>
	static void UpdatePolicyTPS(NetworkEngineT* engine, PolicyData& data);
	
	// 세션 관련 (클라이언트는 단순)
	static SessionId GetSessionId(PolicyData& data);
	static Session* ValidateSession(PolicyData& data);
	static bool ReleaseSession(PolicyData& data);
};