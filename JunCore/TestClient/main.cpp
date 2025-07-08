#include "TestClientHandler.h"
#include "TestClientManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <future>
#include <vector>

//==============================================================================
// Connection Test Functions
//==============================================================================

void RunConnectionTest(const std::string& address, Port port)
{
    Logger::Info("Running connection stress test...");

    const int kNumClients = 5;
    const int kMessagesPerClient = 10;

    std::vector<std::future<void>> client_futures;

    for (int i = 0; i < kNumClients; ++i)
    {
        client_futures.push_back(
            std::async(std::launch::async, [i, address, port, kMessagesPerClient]()
            {
                TestClientHandler handler;
                auto client = NetworkFactory::CreateClient();

                NetworkError result = client->Connect(address, port, &handler, 5000);
                if (result != NetworkError::kSuccess)
                {
                    Logger::Error("Client " + std::to_string(i) + " failed to connect");
                    return;
                }

                Logger::Info("Client " + std::to_string(i) + " connected");

                for (int j = 0; j < kMessagesPerClient; ++j)
                {
                    std::string message = "Client" + std::to_string(i) + "_Message" + std::to_string(j);
                    client->Send(message);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                std::this_thread::sleep_for(std::chrono::seconds(2));
                client->Disconnect();
                Logger::Info("Client " + std::to_string(i) + " disconnected");
            })
        );
    }

    // Wait for all clients to complete
    for (auto& future : client_futures)
    {
        future.wait();
    }

    Logger::Info("Connection stress test completed");
}


//==============================================================================
// Main Function
//==============================================================================

int main(int argc, char* argv[])
{
    try
    {
        // Configure logging
        auto console_logger = std::make_shared<ConsoleLogger>();
        console_logger->SetMinLevel(LogLevel::kInfo);
        Logger::SetLogger(console_logger);

        Logger::Info("JunCore3 Test Client starting...");

        // Parse command line arguments
        std::string server_address = "127.0.0.1";
        Port server_port = 9000;
        bool automated_test = false;
        bool stress_test = false;

        if (argc >= 2)
        {
            server_address = argv[1];
        }
        if (argc >= 3)
        {
            server_port = static_cast<Port>(std::stoi(argv[2]));
        }
        if (argc >= 4)
        {
            std::string mode = argv[3];
            if (mode == "auto")
            {
                automated_test = true;
            }
            else if (mode == "stress")
            {
                stress_test = true;
            }
        }

        // Validate inputs
        if (!NetworkUtils::IsValidIPAddress(server_address) && server_address != "localhost")
        {
            // Try to resolve hostname
            auto addresses = NetworkUtils::ResolveHostname(server_address);
            if (addresses.empty())
            {
                Logger::Error("Invalid server address: " + server_address);
                return 1;
            }
            server_address = addresses[0];
        }

        if (!NetworkUtils::IsValidPort(server_port))
        {
            Logger::Error("Invalid port number: " + std::to_string(server_port));
            return 1;
        }

        std::cout << "\n=== JunCore3 Test Client ===" << std::endl;
        std::cout << "Target server: " << server_address << ":" << server_port << std::endl;

        if (stress_test)
        {
            RunConnectionTest(server_address, server_port);
            return 0;
        }

        // Create and connect client
        TestClientManager client_manager;

        if (!client_manager.Connect(server_address, server_port))
        {
            Logger::Error("Failed to connect to server");
            return 1;
        }

        if (automated_test)
        {
            client_manager.RunAutomatedTest();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        else
        {
            client_manager.RunInteractiveSession();
        }

        // Cleanup
        client_manager.Disconnect();
        Logger::Info("Client shutdown complete.");

    }
    catch (const std::exception& e)
    {
        Logger::Error("Client error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
