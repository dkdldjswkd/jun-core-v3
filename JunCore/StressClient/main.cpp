#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
#include "StressClient.h"
#include "../EchoServer/echo_message.pb.h"
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

        LOG_INFO("Starting stress test in 3 seconds...");
        Sleep(3000);

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