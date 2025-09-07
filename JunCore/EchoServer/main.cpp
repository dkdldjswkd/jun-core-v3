#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/protocol/handshake.pb.h"
#include "../JunCore/network/IOCPManager.h"
using namespace std;

static CrashDump dump;

int main() 
{
	try
	{
		// IOCPManager 생성 (워커 스레드 3개)
		auto iocpManager = IOCPManager::Create().WithWorkerCount(3).Build();
		
		if (!iocpManager->IsValid()) 
		{
			LOG_ERROR("Failed to create IOCPManager");
			return -1;
		}

		// EchoServer 생성 및 IOCP 연결 (shared_ptr로 변환)
		EchoServer echoServer;
		echoServer.AttachIOCPManager(std::shared_ptr<IOCPManager>(std::move(iocpManager)));

		// 서버 시작 (IP: 0.0.0.0, Port: 7777, Max Sessions: 1000)
		if (echoServer.StartServer("0.0.0.0", 7777, 1000)) 
		{
			LOG_INFO("EchoServer started successfully on port 7777");
		} 
		else 
		{
			LOG_ERROR("Failed to start EchoServer");
			return -1;
		}

		for (;;)
		{
			Sleep(1000);
		}

		echoServer.StopServer();
		echoServer.DetachIOCPManager();
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