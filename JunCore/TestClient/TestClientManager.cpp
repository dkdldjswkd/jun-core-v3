#include "TestClientManager.h"
#include <iostream>
#include <thread>
#include <chrono>

TestClientManager::TestClientManager() : client_(NetworkFactory::CreateClient())
{
}

bool TestClientManager::Connect(const std::string& address, Port port)
{
    Logger::Info("Connecting to " + address + ":" + std::to_string(port) + "...");

    NetworkError result = client_->Connect(address, port, &handler_, 5000);
    if (result != NetworkError::kSuccess)
    {
        Logger::Error("Failed to connect: " + NetworkUtils::GetErrorMessage(result));
        return false;
    }

    Logger::Info("Connected successfully!");
    return true;
}

void TestClientManager::Disconnect()
{
    Logger::Info("Disconnecting...");
    client_->Disconnect();
}

void TestClientManager::RunInteractiveSession()
{
    std::string input;
    ShowHelp();

    while (client_->IsConnected())
    {
        std::cout << "\nClient> ";
        std::getline(std::cin, input);

        if (input == "quit")
        {
            break;
        }
        else if (input == "status")
        {
            ShowStatus();
        }
        else if (input == "ping")
        {
            SendPing();
        }
        else if (input == "help")
        {
            ShowHelp();
        }
        else if (!input.empty())
        {
            SendMessage(input);
        }
    }
}

void TestClientManager::RunAutomatedTest()
{
    Logger::Info("Running automated test sequence...");

    // Send test messages
    SendMessage("Hello Server!");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    SendPing();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    SendMessage("This is a test message from automated client");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    SendMessage("Test complete");
    Logger::Info("Automated test sequence completed");
}

void TestClientManager::ShowStatus()
{
    std::cout << "=== Client Status ===" << std::endl;
    std::cout << "Connected: " << (client_->IsConnected() ? "Yes" : "No") << std::endl;
    if (client_->IsConnected())
    {
        // Note: We'd need to extend the interface to get remote endpoint info
    }
}

void TestClientManager::SendPing()
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::string ping_message = "PING:" + std::to_string(timestamp);
    NetworkError result = client_->Send(ping_message);

    if (result == NetworkError::kSuccess)
    {
        std::cout << "Ping sent at " << timestamp << std::endl;
    }
    else
    {
        Logger::Error("Failed to send ping: " + NetworkUtils::GetErrorMessage(result));
    }
}

void TestClientManager::SendMessage(const std::string& message)
{
    NetworkError result = client_->Send(message);
    if (result != NetworkError::kSuccess)
    {
        Logger::Error("Failed to send message: " + NetworkUtils::GetErrorMessage(result));
    }
}

void TestClientManager::ShowHelp()
{
    std::cout << "\n=== Available Commands ===" << std::endl;
    std::cout << "  quit    - Disconnect and exit" << std::endl;
    std::cout << "  status  - Show connection status" << std::endl;
    std::cout << "  ping    - Send ping to server" << std::endl;
    std::cout << "  help    - Show this help" << std::endl;
    std::cout << "  <text>  - Send text message to server" << std::endl;
}
