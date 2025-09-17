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

void PrintUsage() 
{
    cout << "=== StressClient Usage ===" << endl;
    cout << "Commands:" << endl;
    cout << "  start  - 스트레스 테스트 시작" << endl;
    cout << "  stop   - 스트레스 테스트 중지" << endl;
    cout << "  stats  - 성능 통계 출력" << endl;
    cout << "  info   - 현재 상태 정보" << endl;
    cout << "  exit   - 프로그램 종료" << endl;
    cout << "=========================" << endl;
}

void PrintTestInfo()
{
    cout << "=== Stress Test Configuration ===" << endl;
    cout << "Session Count: " << SESSION_COUNT << endl;
    cout << "Message Interval: " << MESSAGE_INTERVAL_MS << "ms" << endl;
    cout << "Message Size Range: " << MESSAGE_MIN_SIZE << "-" << MESSAGE_MAX_SIZE << " chars" << endl;
    cout << "Server: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "=================================" << endl;
}

void PrintStats(const StressClient& client)
{
    cout << "=== Performance Statistics ===" << endl;
    cout << "Test Running: " << (client.IsRunning() ? "YES" : "NO") << endl;
    
    // 향후 통계 기능 확장 가능
    cout << "총 연결 세션: " << SESSION_COUNT << endl;
    cout << "초당 예상 메시지: " << (1000 / MESSAGE_INTERVAL_MS * SESSION_COUNT) << " msg/s" << endl;
    cout << "==============================" << endl;
}

int main() 
{
    try
    {
        cout << "=== JunCore Stress Test Client ===" << endl;
        cout << "서버에 대한 고강도 스트레스 테스트를 수행합니다." << endl;
        cout << "연속적인 메시지 전송과 Echo 응답 순서 검증을 통해" << endl;
        cout << "네트워크 코어의 안정성과 성능을 검증합니다." << endl;
        cout << "==================================" << endl << endl;

        // IOCP Manager 생성 (스트레스 테스트용으로 워커 스레드 수 증가)
        auto iocpManager = IOCPManager::Create().WithWorkerCount(4).Build();
        LOG_ERROR_RETURN(iocpManager->IsValid(), -1, "Failed to create IOCPManager for stress client");

        // StressClient 생성 및 초기화
        StressClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)));
        client.Initialize();
        
        PrintTestInfo();
        PrintUsage();

        // 메인 커맨드 루프
        while (true)
        {
            string input;
            cout << "[StressClient]>> ";

            if (!getline(cin, input)) {
                LOG_INFO("Input error or EOF. Exiting...");
                break;
            }

            if (input.empty()) {
                continue;
            }

            // 명령어 처리
            if (input == "start") 
            {
                if (client.IsRunning()) {
                    cout << "[WARNING] 스트레스 테스트가 이미 실행 중입니다!" << endl;
                } else {
                    cout << "[INFO] 스트레스 테스트를 시작합니다..." << endl;
                    client.StartStressTest();
                    cout << "[INFO] " << SESSION_COUNT << "개 세션으로 스트레스 테스트 시작됨!" << endl;
                    cout << "[INFO] 'stats' 명령으로 상태 확인, 'stop' 명령으로 중지 가능" << endl;
                }
            }
            else if (input == "stop") 
            {
                if (!client.IsRunning()) {
                    cout << "[WARNING] 스트레스 테스트가 실행되지 않고 있습니다!" << endl;
                } else {
                    cout << "[INFO] 스트레스 테스트를 중지합니다..." << endl;
                    client.StopStressTest();
                    cout << "[INFO] 스트레스 테스트가 중지되었습니다." << endl;
                }
            }
            else if (input == "stats") 
            {
                PrintStats(client);
            }
            else if (input == "info") 
            {
                PrintTestInfo();
                PrintStats(client);
            }
            else if (input == "help") 
            {
                PrintUsage();
            }
            else if (input == "exit") 
            {
                cout << "[INFO] 프로그램을 종료합니다..." << endl;
                if (client.IsRunning()) {
                    cout << "[INFO] 실행 중인 스트레스 테스트를 먼저 중지합니다..." << endl;
                    client.StopStressTest();
                }
                break;
            }
            else 
            {
                cout << "[ERROR] 알 수 없는 명령어: '" << input << "'" << endl;
                cout << "'help' 명령으로 사용법을 확인하세요." << endl;
            }
        }

        LOG_INFO("Stress client shutting down...");
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