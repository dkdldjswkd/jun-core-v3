#include "GameServerManager.h"
#include <iostream>
#include <thread>
#include <chrono>

GameServerManager::GameServerManager() : server_(NetworkFactory::CreateServer())
{
}

bool GameServerManager::Start(Port port)
{
    Logger::Info("Starting JunCore3 Game Server...");

    NetworkError result = server_->Start(port, &handler_, 0);
    if (result != NetworkError::kSuccess)
    {
        Logger::Error("Failed to start server: " + NetworkUtils::GetErrorMessage(result));
        return false;
    }

    Logger::Info("Server started successfully on port " + std::to_string(port));
    return true;
}

void GameServerManager::Stop()
{
    Logger::Info("Stopping server...");
    server_->Stop();
}

void GameServerManager::RunConsoleCommands()
{
    std::string command;
    while (server_->IsRunning())
    {
        std::cout << "\nServer Console Commands:" << std::endl;
        std::cout << "  status  - Show server status" << std::endl;
        std::cout << "  clients - List connected clients" << std::endl;
        std::cout << "  broadcast <message> - Broadcast to all clients" << std::endl;
        std::cout << "  quit    - Stop server" << std::endl;
        std::cout << "Enter command: ";

        std::getline(std::cin, command);

        if (command == "quit")
        {
            break;
        }
        else if (command == "status")
        {
            ShowStatus();
        }
        else if (command == "clients")
        {
            ShowClients();
        }
        else if (command.substr(0, 9) == "broadcast")
        {
            if (command.length() > 10)
            {
                std::string message = command.substr(10);
                BroadcastMessage(message);
            }
            else
            {
                std::cout << "Usage: broadcast <message>" << std::endl;
            }
        }
        else if (!command.empty())
        {
            std::cout << "Unknown command: " << command << std::endl;
        }
    }
}

void GameServerManager::ShowStatus()
{
    std::cout << "=== Server Status ===" << std::endl;
    std::cout << "Running: " << (server_->IsRunning() ? "Yes" : "No") << std::endl;
    std::cout << "Connected clients: " << server_->GetClientCount() << std::endl;
    std::cout << "Local IP: " << NetworkUtils::GetLocalIPAddress() << std::endl;
}

void GameServerManager::ShowClients()
{
    size_t client_count = server_->GetClientCount();
    std::cout << "=== Connected Clients (" << client_count << ") ===" << std::endl;

    if (client_count == 0)
    {
        std::cout << "No clients connected." << std::endl;
    }
    // Note: For detailed client listing, we'd need to extend the interface
}

void GameServerManager::BroadcastMessage(const std::string& message)
{
    std::string broadcast_msg = "[Server Broadcast] " + message;
    server_->BroadcastToAllClients(broadcast_msg);
    std::cout << "Broadcasted message to " << server_->GetClientCount() << " clients." << std::endl;
}
