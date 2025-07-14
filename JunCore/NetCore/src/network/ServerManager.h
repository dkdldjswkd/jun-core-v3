#pragma once
#include "Session.h"
#include "IocpManager.h"
#include <memory>
#include <thread>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <WinSock2.h>
#include <MSWSock.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

class ServerManager
{
public:
	ServerManager();
	~ServerManager();

public:
	bool StartServer(std::string const& _bind_ip, uint16 _port, int _worker_cnt);
	void StopServer();

	// Event Handlers
	std::function<void(SessionPtr)> OnAccept = nullptr;
	std::function<void(SessionPtr)> OnSessionClose = nullptr;

private:
	void AsyncAccept();

public:
	template<typename T>
	void RegisterPacketHandler(int32 _packet_id, std::function<void(SessionPtr, const T&)> _packet_handle)
	{
		packet_handler_[_packet_id] = [_packet_handle](SessionPtr _session_ptr, const std::vector<char>& _serialized_packet) {
			T message;
			if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size())) {
				_packet_handle(_session_ptr, message);
			}
		};
	}

	void HandlePacket(SessionPtr session, int32 packet_id, const std::vector<char>& _serialized_packet) 
	{
		auto it = packet_handler_.find(packet_id);
		if (it != packet_handler_.end())
		{
			it->second(session, _serialized_packet);
		}
		else
		{
			// LOG_ERROR
			std::cout << "HandlePacket Error!!!!" << std::endl;
		}
	}

private:
	// IOCP Manager
	IocpManager iocp_manager_;

	// Listener Socket
	SOCKET listen_socket_;
	std::vector<char> accept_buffer_;
	LPFN_ACCEPTEX lpfnAcceptEx_;
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs_;

	// Accept Context
	IocpContext accept_context_;

	// packet handler
	std::unordered_map<int32, std::function<void(SessionPtr /*session ptr*/, const std::vector<char>&/*serialized packet*/)>> packet_handler_;
};