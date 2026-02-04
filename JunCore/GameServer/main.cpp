#include <cstdlib>
#include <iostream>
#include "GameServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
using namespace std;

static CrashDump dump;

void log(const GameServer& server);

int main()
{
	try
	{
		// Logger 초기화
		LOGGER_INITIALIZE_SYNC(LOG_LEVEL_INFO);

		// IOCP Manager 생성 (Worker 스레드 4개)
		auto iocpManager = IOCPManager::Create().WithWorkerCount(4).WithMonitoring(true).Build();

		if (!iocpManager->IsValid())
		{
			LOG_ERROR("Failed to create IOCPManager");
			return -1;
		}

		// GameServer 생성 (GameThread 2개)
		GameServer gameServer(std::shared_ptr<IOCPManager>(std::move(iocpManager)), 2);
		gameServer.Initialize();

		LOG_INFO("Server will start in 3 seconds");
		Sleep(3000);

		if (gameServer.StartServer("0.0.0.0"/*IP*/, 8888/*Port*/, 1000 /*Max session*/))
		{
			LOG_INFO("GameServer started on port 8888");
		}
		else
		{
			LOG_ERROR("Failed to start GameServer");
			return -1;
		}

		bool isProfileMode = false;

		// 메인 루프: 1초마다 통계 출력
		while(true)
		{
			Sleep(1000);

			if (isProfileMode)
			{
				std::system("cls");
				log(gameServer);
			}
		}

		gameServer.StopServer();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Exception caught in main: %s", e.what());
	}
	catch (...)
	{
		LOG_ERROR("Unknown exception caught in main");
	}
}

void log(const GameServer& server)
{
	printf("=== GameServer 세션 통계 ===\n"
		"현재 접속중인 세션 수: %u\n"
		"누적 연결된 세션 수: %u\n"
		"누적 끊어진 세션 수: %u\n"
		"=== 네트워크 통계 (10초) ===\n"
		"수신 속도: %.2f KB/s\n"
		"송신 속도: %.2f KB/s\n"
		"========================\n",
		server.GetCurrentSessions(),
		server.GetTotalConnected(),
		server.GetTotalDisconnected(),
		server.GetRecvBytesPerSecond(10) / 1024.0,
		server.GetSendBytesPerSecond(10) / 1024.0);
}
