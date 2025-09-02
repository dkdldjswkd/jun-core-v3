#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/protocol/handshake.pb.h"
#include "../JunCore/network/NetworkArchitecture.h"
using namespace std;

static CrashDump dump;

// EchoServer.cpp, Define 확인
void StartEchoServer()
{
	try 
	{
		INIT_NETWORK_ARCH(5);  // 5개 워커 스레드
		
		auto echoServer = CREATE_HANDLER(EchoServer);
		
		// 3. 서버 시작
		printf("3. Starting EchoServer...\n");
		echoServer->Start();
		printf("EchoServer started successfully with new architecture!\n");

		printf("\nEchoServer started - Waiting for packets...\n");
		printf("Only RECV/SEND messages will be displayed\n");
		printf("--------------------------------------------------------\n");
		
		for (;;)
		{
			Sleep(1000);
			// 대기 중
		}

		printf("\nShutting down...\n");
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