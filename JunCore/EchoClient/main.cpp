#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
#include "EchoClient.h"
#include "../EchoServer/echo_message.pb.h"
#include <iostream>
#include <string>
using namespace std;

static CrashDump dump;

int main() 
{
	try
	{
		// Logger 초기화 (비동기 모드)
		LOGGER_INITIALIZE_ASYNC(LOG_LEVEL_DEBUG);

		auto iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		LOG_ERROR_RETURN(iocpManager->IsValid(), -1, "Failed to create IOCPManager for client");

		EchoClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)), "127.0.0.1", 7777, 1);

		client.Initialize();
		client.StartClient();  // OnClientStart()에서 콘솔 입력 스레드 시작됨

		//LOG_INFO("EchoClient started. Press Enter to stop...");
		//cin.get();
		for (;;)
		{
			Sleep(INFINITE);
		}

		LOG_INFO("Stopping client...");
		client.StopClient();  // OnClientStop()에서 콘솔 입력 스레드 종료됨
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Exception caught in client: %s", e.what());
		return -1;
	}
	catch (...)
	{
		LOG_ERROR("Unknown exception caught in client");
		return -1;
	}

	return 0;
}