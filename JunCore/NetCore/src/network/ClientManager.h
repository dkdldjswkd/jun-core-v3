#pragma once
#include "Session.h"
#include "IocpManager.h"
#include <string>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <map>
#include <WinSock2.h>
#include <MSWSock.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

class ClientManager
{
public:
	ClientManager();
	~ClientManager();

	bool Connect(const std::string& ip_address, uint16 port);
	void Disconnect();

	void SendPacket(int32 packet_id, const std::vector<char>& serialized_packet);

	// Client methods
	bool StartClient(int worker_threads);
	void StopClient();
	
	// Packet handler support
	template<typename T>
	void RegisterPacketHandler(int32 packet_id, std::function<void(SessionPtr, const T&)> handler);
	void HandlePacket(SessionPtr session_ptr, int32 packet_id, const std::vector<char>& data);

	// Event Handlers
	std::function<void(SessionPtr)> OnConnect = nullptr;
	std::function<void(SessionPtr)> OnDisconnect = nullptr;
	std::function<void(SessionPtr, int32, const std::vector<char>&)> OnPacketReceive = nullptr;

private:
	void HandleConnectCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped);
	void HandleRecvCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped);
	void HandleSendCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped);

	void AsyncRecv();
	void AsyncSend();

	IocpManager iocp_manager_;
	SessionPtr session_;
	SOCKET client_socket_;

	// Connect Context
	IocpContext connect_context_;
	LPFN_CONNECTEX lpfnConnectEx_;

	// WSABUFs for send/recv
	WSABUF recv_wsa_buf_;
	WSABUF send_wsa_buf_;

	// Message buffer for receiving data
	MessageBuffer recv_buffer_;

	// Send queue
	std::queue<PacketBufferPtr> send_queue_;
	std::mutex send_queue_lock_;
	
	// Packet handlers
	std::map<int32, std::function<void(SessionPtr, const std::vector<char>&)>> packet_handlers_;
	std::mutex packet_handlers_lock_;
};

template<typename T>
inline void ClientManager::RegisterPacketHandler(int32 packet_id, std::function<void(SessionPtr, const T&)> handler)
{
	std::lock_guard<std::mutex> lock(packet_handlers_lock_);
	packet_handlers_[packet_id] = [handler](SessionPtr session_ptr, const std::vector<char>& data) {
		T packet;
		if (packet.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
			handler(session_ptr, packet);
		}
	};
}