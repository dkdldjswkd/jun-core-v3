#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
#include "StressClient.h"
#include "echo_message.pb.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
using namespace std;
using namespace std::chrono;

static CrashDump dump;

int main() 
{
    try
    {
        // Logger 초기화 (비동기 모드)
        LOGGER_INITIALIZE_SYNC(LOG_LEVEL_WARN);
        
        auto iocpManager = IOCPManager::Create().WithWorkerCount(4).Build();

        StressClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)));
        client.Initialize();

        LOG_INFO("=== Stress Test Configuration ===\n"
                 "Session Count: %d\n"
                 "Message Interval: %dms\n"
                 "Message Size Range: %d-%d chars\n"
                 "Server: %s:%d\n"
                 "=================================",
                 SESSION_COUNT, MESSAGE_INTERVAL_MS, MESSAGE_MIN_SIZE, MESSAGE_MAX_SIZE, SERVER_IP.c_str(), SERVER_PORT);

        LOG_INFO("Starting in 3 seconds…");
        Sleep(3000);

        // StartClient() 호출 → OnClientStart()에서 워커 스레드 시작
        // 하지만 자동 재연결은 사용하지 않음 (targetConnectionCount=1이므로)
        client.StartClient();

        LOG_INFO("Stress test running. Press Enter to stop...");
        cin.get();

        LOG_INFO("Stopping stress test...");
        client.StopClient();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception caught in stress client: %s", e.what());
        return -1;
    }
    catch (...)
    {
        LOG_ERROR("Unknown exception caught in stress client");
        return -1;
    }

    return 0;
}