#include <cstdlib>
#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
using namespace std;

static CrashDump dump;

void log(const EchoServer& server);

int main() 
{
	try
	{
		auto iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		
		if (!iocpManager->IsValid()) 
		{
			LOG_ERROR("Failed to create IOCPManager");
			return -1;
		}

		EchoServer echoServer(std::shared_ptr<IOCPManager>(std::move(iocpManager)));
		echoServer.Initialize();

		LOG_INFO("Server will start in 3 seconds");
		Sleep(3000);

		if (echoServer.StartServer("0.0.0.0"/*IP*/, 7777/*Port*/, 1000 /*Max session*/))
		{
			LOG_INFO("EchoServer started");
		}
		else
		{
			LOG_ERROR("Failed to start EchoServer");
			return -1;
		}

		for (;;)
		{
			Sleep(1000);
			std::system("cls");

			log(echoServer);
		}

		echoServer.StopServer();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Exception caught in StartEchoServer: %s", e.what());
	}
	catch (...)
	{
		LOG_ERROR("Unknown exception caught in StartEchoServer");
	}
}

void log(const EchoServer& server)
{
	printf("=== EchoServer 세션 통계 ===\n"
		"현재 접속중인 세션 수: %u\n"
		"누적 연결된 세션 수: %u\n"
		"누적 끊어진 세션 수: %u\n"
		"========================\n",
		server.GetCurrentSessions(),
		server.GetTotalConnected(),
		server.GetTotalDisconnected());
}