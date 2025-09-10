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
		std::unique_ptr<IOCPManager> iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		
		if (!iocpManager->IsValid()) 
		{
			LOG_ERROR("Failed to create IOCPManager");
			return -1;
		}

		EchoServer echoServer;
		
		// 엔진 초기화 (패킷 핸들러 등록)
		echoServer.Initialize();
		
		echoServer.AttachIOCPManager(std::shared_ptr<IOCPManager>(std::move(iocpManager)));

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
		}

		echoServer.StopServer();
		echoServer.DetachIOCPManager();
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