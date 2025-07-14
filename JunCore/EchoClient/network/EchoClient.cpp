#include "EchoClient.h"
#include "../packet/packet.pb.h"
#include <iostream>

EchoClient& EchoClient::Instance()
{
	static EchoClient _instance;
	return _instance;
}

bool EchoClient::StartClient(int workerThreads)
{
	InitPacketHandlers();
	
	client_manager_.OnConnect = [this](SessionPtr session_ptr) {
		OnConnect(session_ptr);
	};
	
	client_manager_.OnDisconnect = [this](SessionPtr session_ptr) {
		OnDisconnect(session_ptr);
	};
	
	if (!client_manager_.StartClient(workerThreads))
	{
		std::cerr << "Failed to start EchoClient" << std::endl;
		return false;
	}
	
	std::cout << "EchoClient started." << std::endl;
	return true;
}

void EchoClient::StopClient()
{
	client_manager_.StopClient();
	std::cout << "EchoClient stopped." << std::endl;
}

bool EchoClient::Connect(const std::string& ip, uint16 port)
{
	if (!client_manager_.Connect(ip, port))
	{
		std::cerr << "Failed to connect to server " << ip << ":" << port << std::endl;
		return false;
	}
	
	std::cout << "Connecting to server " << ip << ":" << port << std::endl;
	return true;
}

void EchoClient::Disconnect()
{
	client_manager_.Disconnect();
	std::cout << "Disconnected from server." << std::endl;
}

void EchoClient::OnConnect(SessionPtr session_ptr)
{
	std::lock_guard<std::mutex> _lock(session_mutex_);
	session_ptr_ = session_ptr;
	
	session_ptr->OnPacketReceiveCallback = [this](SessionPtr session_ptr, int32 packet_id, const std::vector<char>& data) {
		client_manager_.HandlePacket(session_ptr, packet_id, data);
	};
	
	std::cout << "Connected to server successfully." << std::endl;
}

void EchoClient::OnDisconnect(SessionPtr session_ptr)
{
	std::lock_guard<std::mutex> _lock(session_mutex_);
	session_ptr_ = nullptr;
	std::cout << "Disconnected from server." << std::endl;
}

void EchoClient::InitPacketHandlers()
{
    client_manager_.RegisterPacketHandler<PacketLib::GU_ECHO_RES>(
        static_cast<int32>(PacketLib::PACKET_ID::PACKET_ID_GU_ECHO_RES),
        [this](SessionPtr _session_ptr, const PacketLib::GU_ECHO_RES& _packet) 
        {
			std::cout << "Echo response received: " << _packet.echo() << std::endl;
        }
    );
}