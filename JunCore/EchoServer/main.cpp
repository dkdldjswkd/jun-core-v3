#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/protocol/handshake.pb.h"
using namespace std;

// EchoServer.cpp, Define 확인
void StartEchoServer()
{
	try 
	{
		printf("Creating EchoServer...\n");
		EchoServer server("../ServerConfig.ini", "EchoServer");
		
		printf("Starting EchoServer...\n");
		server.Start();
		printf("EchoServer started successfully on port 7777\n");

		// TPS 모니터링 옵션 (true: 활성화, false: 비활성화)
		bool enableTpsMonitoring = false;
		
		if (enableTpsMonitoring) {
			for (;;)
			{
				// 1초 주기로 TPS 출력
				Sleep(1000);
				printf("NetworkLib ---------------------------------------------------- \n");
				printf("세션 수          : %d \n", server.GetSessionCount());
				printf("패킷 수          : %d \n", 0); // PacketBuffer::Get_UseCount() 사용 불가
				printf("총 접속 수       : %d \n", server.GetAcceptTotal());
				printf("초당 접속 수     : %d \n", server.GetAcceptTPS());
				printf("초당 전송 메시지 : %d \n", server.GetSendTPS());
				printf("초당 수신 메시지 : %d \n", server.GetRecvTPS());
				printf("\n\n\n\n\n\n\n\n\n\n \n\n\n\n\n\n\n\n\n\n \n\n");
			}
		} else {
			// TPS 모니터링 없이 조용히 대기 (RECV/SEND 로그만 표시)
			printf("EchoServer started. Monitoring RECV/SEND messages only...\n");
			for (;;)
			{
				Sleep(1000);
				// TPS 업데이트만 수행 (출력 없음)
				server.UpdateTPS();
			}
		}

		server.Stop();
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