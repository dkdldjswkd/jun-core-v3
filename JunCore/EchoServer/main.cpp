#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/protocol/handshake.pb.h"
#include "../JunCore/network/NetworkArchitecture.h"
using namespace std;

// EchoServer.cpp, Define 확인
void StartEchoServer()
{
	try 
	{
		printf("🌟 === New Network Architecture Demo === 🌟\n");
		
		// 1. 새로운 NetworkArchitecture 초기화
		printf("1. Initializing NetworkArchitecture...\n");
		INIT_NETWORK_ARCH(5);  // 5개 워커 스레드
		
		// 2. EchoServer 핸들러 생성
		printf("2. Creating EchoServer handler...\n");
		auto echoServer = CREATE_HANDLER(EchoServer, "../ServerConfig.ini", "EchoServer");
		
		// 3. 서버 시작
		printf("3. Starting EchoServer...\n");
		echoServer->Start();
		printf("✅ EchoServer started successfully with new architecture!\n");

		// TPS 모니터링 비활성화 - RECV/SEND 로그만 표시
		printf("\n📡 EchoServer started - Waiting for packets...\n");
		printf("💡 Only RECV/SEND messages will be displayed\n");
		printf("--------------------------------------------------------\n");
		
		for (;;)
		{
			Sleep(1000);
			// TPS 업데이트만 수행 (출력 없음)
			NETWORK_ARCH.UpdateAllTPS();
		}

		printf("\n🛑 Shutting down...\n");
		echoServer->Stop();
		SHUTDOWN_NETWORK_ARCH();
	}
	catch (const std::exception& e) 
	{
		printf("Exception caught in StartEchoServer: %s\n", e.what());
	}
	catch (...) 
	{
		printf("Unknown exception caught in StartEchoServer\n");
	}
}

int main() 
{
	printf("=== MAIN FUNCTION STARTED ===\n");
	fflush(stdout);
	
	//// protocl test
	//{
	//	jun_core::CS_CONNECT_REQ cs_connect_req;
	//	cs_connect_req.set_protocol_code(123);
	//	std::cout << "Protocol Code: " << cs_connect_req.protocol_code() << std::endl;
	//}

	try {
		printf("=== Initializing CrashDump ===\n");
		fflush(stdout);
		static CrashDump dump;
		
		printf("=== Starting EchoServer ===\n");
		fflush(stdout);
		StartEchoServer();
		
		printf("=== Server finished, sleeping ===\n");
		fflush(stdout);
		Sleep(INFINITE);
	}
	catch (const std::exception& e) {
		printf("!!! Exception in main: %s !!!\n", e.what());
		fflush(stdout);
	}
	catch (...) {
		printf("!!! Unknown exception in main !!!\n");
		fflush(stdout);
	}
}