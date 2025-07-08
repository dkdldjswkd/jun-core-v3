#include "GameServerHandler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>

void GameServerHandler::OnConnected(std::shared_ptr<IConnection> connection)
{
    Logger::Info("Client connected from " + connection->GetRemoteAddress() +
                 ":" + std::to_string(connection->GetRemotePort()) +
                 " (ID: " + std::to_string(connection->GetId()) + ")");
}

void GameServerHandler::OnDisconnected(std::shared_ptr<IConnection> connection)
{
    Logger::Info("Client disconnected (ID: " + std::to_string(connection->GetId()) + ")");
}

void GameServerHandler::OnDataReceived(std::shared_ptr<IConnection> connection,
                                        const char* data, size_t length)
{
    std::string message(data, length);
    Logger::Info("Received from client " + std::to_string(connection->GetId()) +
                 ": " + message);

    // Echo the message back to the client
    std::string echo_response = "Echo: " + message;
    NetworkError result = connection->Send(echo_response);

    if (result != NetworkError::kSuccess)
    {
        Logger::Error("Failed to send echo response to client " +
                      std::to_string(connection->GetId()));
    }
}

void GameServerHandler::OnError(std::shared_ptr<IConnection> connection,
                                NetworkError error, const std::string& message)
{
    Logger::Error("Connection error for client " +
                  std::to_string(connection->GetId()) +
                  ": " + NetworkUtils::GetErrorMessage(error) +
                  " - " + message);
}

void GameServerHandler::OnClientAccepted(std::shared_ptr<IConnection> client)
{
    Logger::Debug("New client accepted, preparing connection...");

    // Send welcome message
    std::string welcome = "Welcome to JunCore3 Game Server!";
    client->Send(welcome);
}
