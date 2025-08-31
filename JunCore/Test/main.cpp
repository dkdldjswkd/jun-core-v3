#include <iostream>
#include <string>
#include <limits>
#include "ProtobufExample.h"

// 테스트 함수 선언
void TestAES();
void TestRSA();
void RunHandshakeSimulation();
int packet_test();
void RunJobQueueTests();

void ShowMainMenu()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "      JunCore Crypto Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Select test to run:" << std::endl;
    std::cout << "  1. RSA-2048 Encryption Test" << std::endl;
    std::cout << "  2. AES-128 Encryption Test" << std::endl;
    std::cout << "  3. Protobuf Example Test" << std::endl;
    std::cout << "  4. Handshake Simulation Test" << std::endl;
    std::cout << "  5. Packet Test" << std::endl;
    std::cout << "  6. JobQueue/ThreadPool Test" << std::endl;
    std::cout << "  7. Run All Tests" << std::endl;
    std::cout << "  0. Exit" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Enter your choice (0-7): ";
}

void ClearInputBuffer()
{
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void PressAnyKeyToContinue()
{
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

int main()
{
    int choice;
    bool exitProgram = false;

    while (!exitProgram) {
        ShowMainMenu();
        
        if (!(std::cin >> choice)) {
            std::cout << "\nInvalid input! Please enter a number.\n" << std::endl;
            ClearInputBuffer();
            continue;
        }
        
        ClearInputBuffer(); // 입력 버퍼 정리

        switch (choice) {
            case 1:
                std::cout << "\n[RUNNING] RSA-2048 Encryption Test\n" << std::endl;
                TestRSA();
                PressAnyKeyToContinue();
                break;
                
            case 2:
                std::cout << "\n[RUNNING] AES-128 Encryption Test\n" << std::endl;
                TestAES();
                PressAnyKeyToContinue();
                break;
                
            case 3:
                std::cout << "\n[RUNNING] Protobuf Example Test\n" << std::endl;
                ProtobufExample();
                PressAnyKeyToContinue();
                break;
                
            case 4:
                std::cout << "\n[RUNNING] Handshake Simulation Test\n" << std::endl;
                RunHandshakeSimulation();
                PressAnyKeyToContinue();
                break;
                
            case 5:
                std::cout << "\n[RUNNING] Packet Test\n" << std::endl;
                packet_test();
                PressAnyKeyToContinue();
                break;
                
            case 6:
                std::cout << "\n[RUNNING] JobQueue/ThreadPool Test\n" << std::endl;
                RunJobQueueTests();
                PressAnyKeyToContinue();
                break;
                
            case 7:
                std::cout << "\n[RUNNING] All Tests\n" << std::endl;
                
                std::cout << ">>> Starting Protobuf Test..." << std::endl;
                ProtobufExample();
                
                std::cout << "\n>>> Starting RSA Test..." << std::endl;
                TestRSA();
                
                std::cout << "\n>>> Starting AES Test..." << std::endl;
                TestAES();
                
                std::cout << "\n>>> Starting Handshake Simulation..." << std::endl;
                RunHandshakeSimulation();
                
                std::cout << "\n>>> Starting Packet Test..." << std::endl;
                packet_test();
                
                std::cout << "\n>>> Starting JobQueue/ThreadPool Test..." << std::endl;
                RunJobQueueTests();
                
                std::cout << "\n=== All Tests Complete ===" << std::endl;
                PressAnyKeyToContinue();
                break;
                
            case 0:
                std::cout << "\nExiting... Goodbye!" << std::endl;
                exitProgram = true;
                break;
                
            default:
                std::cout << "\nInvalid choice! Please select 0-7.\n" << std::endl;
                break;
        }
    }

    return 0;
}