#include "ClientManager.h"
#include <iostream>
#include <WS2tcpip.h>

// Global Winsock initialization flag (for simplicity, assuming one-time init)
static bool g_winsock_initialized = false;

ClientManager::ClientManager()
	: client_socket_(INVALID_SOCKET),
	  connect_context_(IO_TYPE::IO_CONNECT)
{
	// Initialize Winsock if not already done
	if (!g_winsock_initialized)
	{
		WSADATA wsaData;
		int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (ret != 0)
		{
			std::cerr << "WSAStartup failed: " << ret << std::endl;
		}
		else
		{
			g_winsock_initialized = true;
		}
	}
}

ClientManager::~ClientManager()
{
	Disconnect();
}

bool ClientManager::Connect(const std::string& ip_address, uint16 port)
{
	if (!g_winsock_initialized)
	{
		std::cerr << "Winsock not initialized." << std::endl;
		return false;
	}

	// 1. Create client socket
	client_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (client_socket_ == INVALID_SOCKET)
	{
		std::cerr << "WSASocket failed: " << WSAGetLastError() << std::endl;
		return false;
	}

	// 2. Bind to a local address (required for ConnectEx)
	SOCKADDR_IN local_addr;
	ZeroMemory(&local_addr, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = 0; // Let system choose a port

	if (bind(client_socket_, (SOCKADDR*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR)
	{
		std::cerr << "bind failed for client socket: " << WSAGetLastError() << std::endl;
		closesocket(client_socket_);
		return false;
	}

	// 3. Associate client socket with IOCP
	if (CreateIoCompletionPort((HANDLE)client_socket_, iocp_manager_.GetIocpHandle(), (ULONG_PTR)client_socket_, 0) == NULL)
	{
		std::cerr << "CreateIoCompletionPort for client_socket failed: " << GetLastError() << std::endl;
		closesocket(client_socket_);
		return false;
	}

	// 4. Load ConnectEx function pointer
	GUID GuidConnectEx = WSAID_CONNECTEX;
	DWORD bytes = 0;
	if (WSAIoctl(client_socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidConnectEx, sizeof(GuidConnectEx), &lpfnConnectEx_, sizeof(lpfnConnectEx_), &bytes, NULL, NULL) == SOCKET_ERROR)
	{
		std::cerr << "WSAIoctl(ConnectEx) failed: " << WSAGetLastError() << std::endl;
		closesocket(client_socket_);
		return false;
	}

	// 5. Start IOCP worker threads
	if (!iocp_manager_.Start(0)) // Use default worker count
	{
		std::cerr << "IOCP Manager failed to start for client." << std::endl;
		closesocket(client_socket_);
		return false;
	}

	// 6. Prepare target address
	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	InetPtonA(AF_INET, ip_address.c_str(), &server_addr.sin_addr);
	server_addr.sin_port = htons(port);

	// 7. Initiate ConnectEx
	DWORD bytesSent = 0;
	BOOL ret = lpfnConnectEx_(
		client_socket_,
		(SOCKADDR*)&server_addr,
		sizeof(server_addr),
		NULL, // No send data initially
		0,
		&bytesSent,
		&connect_context_.overlapped
	);

	if (ret == FALSE && (WSAGetLastError() != WSA_IO_PENDING))
	{
		std::cerr << "ConnectEx failed: " << WSAGetLastError() << std::endl;
		closesocket(client_socket_);
		return false;
	}

	// The completion will be handled by IocpManager's worker thread
	return true;
}

void ClientManager::Disconnect()
{
	if (client_socket_ != INVALID_SOCKET)
	{
		closesocket(client_socket_);
		client_socket_ = INVALID_SOCKET;
	}
	iocp_manager_.Stop();
	session_.reset();
}

void ClientManager::SendPacket(int32 packet_id, const std::vector<char>& serialized_packet)
{
	if (!session_ || !session_->IsOpen())
	{
		std::cerr << "Cannot send packet: client not connected or session closed." << std::endl;
		return;
	}

	// Create PacketBuffer and add to send queue
	PacketBufferPtr packet_buffer_ptr = std::make_shared<PacketBuffer>();
	if (packet_buffer_ptr->GetFreeSize() < serialized_packet.size())
	{
		// LOG_ERROR or resize
		std::cerr << "PacketBuffer too small for serialized packet." << std::endl;
		return;
	}

	std::memcpy(packet_buffer_ptr->GetWritePos(), serialized_packet.data(), serialized_packet.size());
	packet_buffer_ptr->MoveWp(serialized_packet.size());
	packet_buffer_ptr->SetHeader(packet_id);

	{
		std::lock_guard<std::mutex> lock(send_queue_lock_);
		send_queue_.push(packet_buffer_ptr);
	}

	AsyncSend();
}

void ClientManager::HandleConnectCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped)
{
	// Update socket context after successful connection
	setsockopt(client_socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	// Create Session object
	session_ = std::make_shared<Session>(client_socket_);

	// Bind callbacks
	session_->OnCloseCallback = OnDisconnect;
	session_->OnPacketReceiveCallback = OnPacketReceive;

	// Start receiving data
	// session_->Start(); // This should be called from IocpManager

	// Notify user
	if (OnConnect)
	{
		OnConnect(session_);
	}

	// Start sending any queued packets
	AsyncSend();
}

void ClientManager::HandleRecvCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped)
{
	if (!session_)
	{
		std::cerr << "Session is null in HandleRecvCompletion." << std::endl;
		return;
	}

	// session_->HandleRecv(transferredBytes); // Private method
}

void ClientManager::HandleSendCompletion(DWORD transferredBytes, LPOVERLAPPED lpOverlapped)
{
	if (!session_)
	{
		std::cerr << "Session is null in HandleSendCompletion." << std::endl;
		return;
	}

	// session_->HandleSend(transferredBytes); // Private method

	// Try to send next packet if any
	AsyncSend();
}

void ClientManager::AsyncRecv()
{
	if (!session_)
		return;

	// session_->AsyncRecv(); // Private method
}

void ClientManager::AsyncSend()
{
	if (!session_)
		return;

	std::lock_guard<std::mutex> lock(send_queue_lock_);
	if (send_queue_.empty())
		return;

	PacketBufferPtr packet_buffer_ptr = send_queue_.front();

	WSABUF wsaBuf;
	wsaBuf.buf = reinterpret_cast<char*>(packet_buffer_ptr->GetPacketPos());
	wsaBuf.len = static_cast<ULONG>(packet_buffer_ptr->GetPacketSize());

	// Send implementation would go here
	// For now, just remove the packet from queue
	send_queue_.pop();
}

bool ClientManager::StartClient(int worker_threads)
{
	if (!iocp_manager_.Start(worker_threads))
	{
		std::cerr << "IOCP Manager failed to start for client." << std::endl;
		return false;
	}
	return true;
}

void ClientManager::StopClient()
{
	iocp_manager_.Stop();
	session_.reset();
}

void ClientManager::HandlePacket(SessionPtr session_ptr, int32 packet_id, const std::vector<char>& data)
{
	std::lock_guard<std::mutex> lock(packet_handlers_lock_);
	auto it = packet_handlers_.find(packet_id);
	if (it != packet_handlers_.end())
	{
		it->second(session_ptr, data);
	}
}
