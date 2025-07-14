#include "GameServerManager.h"
#include <iostream>
#include <thread>
#include <chrono>

GameServerManager::GameServerManager()
{
}

GameServerManager::~GameServerManager()
{
}

bool GameServerManager::Start(uint16 port, int worker_count)
{
	// Bind callbacks from ServerManager to GameServerManager methods
	server_manager_.OnAccept = std::bind(&GameServerManager::OnClientAccepted, this, std::placeholders::_1);
	server_manager_.OnSessionClose = std::bind(&GameServerManager::OnClientDisconnected, this, std::placeholders::_1);

	// Start the server
	return server_manager_.StartServer("0.0.0.0", port, worker_count);
}

void GameServerManager::Stop()
{
	server_manager_.StopServer();
}

void GameServerManager::RunConsoleCommands()
{
	std::string command;
	while (std::getline(std::cin, command))
	{
		if (command == "exit")
		{
			break;
		}
		else
		{
			std::cout << "Unknown command: " << command << std::endl;
		}
	}
}

void GameServerManager::OnClientAccepted(SessionPtr session)
{
	std::cout << "Client connected: " << session->GetRemoteIpAddress() << ":" << session->GetRemotePort() << std::endl;

	// Set packet receive callback for this session
	session->OnPacketReceiveCallback = std::bind(&GameServerManager::OnPacketReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

void GameServerManager::OnClientDisconnected(SessionPtr session)
{
	std::cout << "Client disconnected: " << session->GetRemoteIpAddress() << ":" << session->GetRemotePort() << std::endl;
}

void GameServerManager::OnPacketReceived(SessionPtr session, int32 packet_id, const std::vector<char>& serialized_packet)
{
	std::cout << "Packet received from " << session->GetRemoteIpAddress() << ":" << session->GetRemotePort()
		<< ", Packet ID: " << packet_id << ", Size: " << serialized_packet.size() << std::endl;

	// TODO: Process packet based on packet_id
}
