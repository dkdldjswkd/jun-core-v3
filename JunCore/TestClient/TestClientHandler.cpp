#include "TestClientHandler.h"
#include <iostream>

void TestClientHandler::OnConnected(std::shared_ptr<IConnection> connection)
{
    Logger::Info("Connected to server " + connection->GetRemoteAddress() +
                 ":" + std::to_string(connection->GetRemotePort()));
}

void TestClientHandler::OnDisconnected(std::shared_ptr<IConnection> connection)
{
    Logger::Info("Disconnected from server");
}

void TestClientHandler::OnDataReceived(std::shared_ptr<IConnection> connection,
                                        const char* data, size_t length)
{
    std::string message(data, length);
    std::cout << "Server: " << message << std::endl;
}

void TestClientHandler::OnError(std::shared_ptr<IConnection> connection,
                                NetworkError error, const std::string& message)
{
    Logger::Error("Connection error: " + NetworkUtils::GetErrorMessage(error) +
                  " - " + message);
}

void TestClientHandler::OnConnectionAttempt(const std::string& address, Port port)
{
    Logger::Debug("Attempting connection to " + address + ":" + std::to_string(port));
}

void TestClientHandler::OnConnectionEstablished()
{
    Logger::Debug("Connection established successfully");
}

void TestClientHandler::OnConnectionLost()
{
    Logger::Debug("Connection lost");
}
