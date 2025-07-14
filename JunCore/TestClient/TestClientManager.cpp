#include "TestClientManager.h"
#include <iostream>
#include <thread>
#include <chrono>

TestClientManager::TestClientManager()
{
}

TestClientManager::~TestClientManager()
{
}

bool TestClientManager::Connect(const std::string& ip_address, uint16 port)
{
	client_manager_.OnConnected = std::bind(&TestClientManager::OnConnected, this, std::placeholders::_1);
	client_manager_.OnDisconnected = std::bind(&TestClientManager::OnDisconnected, this, std::placeholders::_1);
	client_manager_.OnPacketReceive = std::bind(&TestClientManager::OnPacketReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	return client_manager_.Connect(ip_address, port);
}

void TestClientManager::Disconnect()
{
	client_manager_.Disconnect();
}

void TestClientManager::RunAutomatedTest()
{
	std::cout << "Running automated test..." << std::endl;
	// Example: Send a dummy packet
	std::string test_message = "Hello from automated test!";
	std::vector<char> data(test_message.begin(), test_message.end());
	client_manager_.SendPacket(1001, data); // Assuming 1001 is a test packet ID
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

void TestClientManager::RunInteractiveSession()
{
	std::cout << "Starting interactive session. Type 'exit' to quit." << std::endl;
	std::string command;
	while (std::getline(std::cin, command))
	{
		if (command == "exit")
		{
			break;
		}
		else
		{
			// Example: Send user input as a chat message
			std::vector<char> data(command.begin(), command.end());
			client_manager_.SendPacket(1002, data); // Assuming 1002 is a chat packet ID
		}
	}
}

void TestClientManager::OnConnected(SessionPtr session)
{
	std::cout << "Connected to server: " << session->GetRemoteIpAddress() << ":" << session->GetRemotePort() << std::endl;
}

void TestClientManager::OnDisconnected(SessionPtr session)
{
	std::cout << "Disconnected from server: " << session->GetRemoteIpAddress() << ":" << session->GetRemotePort() << std::endl;
}

void TestClientManager::OnPacketReceived(SessionPtr session, int32 packet_id, const std::vector<char>& serialized_packet)
{
	std::cout << "Packet received from server: Packet ID: " << packet_id << ", Size: " << serialized_packet.size() << std::endl;
	// For simplicity, just print received data as string
	std::string received_data(serialized_packet.begin(), serialized_packet.end());
	std::cout << "Data: " << received_data << std::endl;
}
