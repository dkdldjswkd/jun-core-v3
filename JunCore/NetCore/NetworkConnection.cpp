#include "NetworkConnection.h"
#include "NetCore.h"
#include <iostream>
#include <WS2tcpip.h>


//==============================================================================
// NetworkConnection Implementation
//==============================================================================

NetworkConnection::NetworkConnection(SOCKET socket, ConnectionId id)
	: socket_(socket),
	connection_id_(id),
	state_(ConnectionState::kConnected),
	remote_port_(0),
	address_cached_(false)
{
	ZeroMemory(&overlapped_, sizeof(overlapped_));
	ZeroMemory(receive_buffer_, sizeof(receive_buffer_));
}

NetworkConnection::~NetworkConnection()
{
	std::lock_guard<std::mutex> lock(socket_mutex_);
	if (socket_ != INVALID_SOCKET)
	{
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
	}
}

ConnectionId NetworkConnection::GetId() const
{
	return connection_id_;
}

SOCKET NetworkConnection::GetSocket() const
{
	std::lock_guard<std::mutex> lock(socket_mutex_);
	return socket_;
}

ConnectionState NetworkConnection::GetState() const
{
	return state_.load();
}

bool NetworkConnection::IsValid() const
{
	return GetState() == ConnectionState::kConnected && GetSocket() != INVALID_SOCKET;
}

NetworkError NetworkConnection::Send(const char* data, size_t length)
{
	if (!data || length == 0)
	{
		return NetworkError::kInvalidParameter;
	}
	return SendInternal(data, length);
}

NetworkError NetworkConnection::Send(const std::string& message)
{
	return Send(message.c_str(), message.length());
}

NetworkError NetworkConnection::Send(const ByteBuffer& buffer)
{
	return Send(buffer.data(), buffer.size());
}

void NetworkConnection::Disconnect()
{
	std::lock_guard<std::mutex> lock(socket_mutex_);

	if (socket_ != INVALID_SOCKET)
	{
		state_ = ConnectionState::kDisconnecting;
		shutdown(socket_, SD_BOTH);
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
		state_ = ConnectionState::kDisconnected;
	}
}

std::string NetworkConnection::GetRemoteAddress() const
{
	if (!address_cached_)
	{
		CacheRemoteAddress();
	}
	return remote_address_;
}

Port NetworkConnection::GetRemotePort() const
{
	if (!address_cached_)
	{
		CacheRemoteAddress();
	}
	return remote_port_;
}

void NetworkConnection::ResetOverlapped()
{
	ZeroMemory(&overlapped_, sizeof(overlapped_));
}

void NetworkConnection::SetState(ConnectionState state)
{
	state_ = state;
}

void NetworkConnection::CacheRemoteAddress() const
{
	std::lock_guard<std::mutex> lock(socket_mutex_);

	if (socket_ == INVALID_SOCKET)
	{
		remote_address_ = "Unknown";
		remote_port_ = 0;
		address_cached_ = true;
		return;
	}

	sockaddr_in remote_addr;
	int addr_len = sizeof(remote_addr);

	if (getpeername(socket_, reinterpret_cast<sockaddr*>(&remote_addr), &addr_len) == 0)
	{
		char addr_str[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &remote_addr.sin_addr, addr_str, INET_ADDRSTRLEN))
		{
			remote_address_ = addr_str;
			remote_port_ = ntohs(remote_addr.sin_port);
		}
		else
		{
			remote_address_ = "Unknown";
			remote_port_ = 0;
		}
	}
	else
	{
		remote_address_ = "Unknown";
		remote_port_ = 0;
	}

	address_cached_ = true;
}

NetworkError NetworkConnection::SendInternal(const char* data, size_t length)
{
	if (!IsValid())
	{
		return NetworkError::kConnectionFailed;
	}

	std::lock_guard<std::mutex> lock(socket_mutex_);

	if (socket_ == INVALID_SOCKET)
	{
		return NetworkError::kConnectionFailed;
	}

	WSABUF send_buffer = {
	  static_cast<ULONG>(length),
	  const_cast<CHAR*>(data)
	};

	DWORD bytes_sent = 0;
	OVERLAPPED send_overlapped = {};

	int result = WSASend(socket_, &send_buffer, 1, &bytes_sent, 0, &send_overlapped, nullptr);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			state_ = ConnectionState::kDisconnected;
			Logger::Error("WSASend failed with error: " + std::to_string(error));
			return NetworkError::kSendFailed;
		}
	}

	return NetworkError::kSuccess;
}

//==============================================================================
// ConnectionManager Implementation
//==============================================================================

void ConnectionManager::AddConnection(std::shared_ptr<NetworkConnection> connection)
{
	if (!connection)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(connections_mutex_);
	connections_[connection->GetId()] = connection;
	Logger::Debug("Connection added: " + std::to_string(connection->GetId()));
}

void ConnectionManager::RemoveConnection(ConnectionId id)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	auto removed_count = connections_.erase(id);
	if (removed_count > 0)
	{
		Logger::Debug("Connection removed: " + std::to_string(id));
	}
}

std::shared_ptr<NetworkConnection> ConnectionManager::FindConnection(ConnectionId id)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	auto it = connections_.find(id);
	return (it != connections_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<NetworkConnection>> ConnectionManager::GetAllConnections()
{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	std::vector<std::shared_ptr<NetworkConnection>> result;
	result.reserve(connections_.size());

	for (const auto& pair : connections_)
	{
		if (pair.second && pair.second->IsValid())
		{
			result.push_back(pair.second);
		}
	}

	return result;
}

void ConnectionManager::ClearConnections()
{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	size_t count = connections_.size();
	connections_.clear();
	Logger::Info("Cleared " + std::to_string(count) + " connections");
}

size_t ConnectionManager::GetConnectionCount() const
{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	return connections_.size();
}

void ConnectionManager::BroadcastToAll(const char* data, size_t length)
{
	auto connections = GetAllConnections();

	for (auto& connection : connections)
	{
		if (connection && connection->IsValid())
		{
			connection->Send(data, length);
		}
	}

	Logger::Debug("Broadcasted message to " + std::to_string(connections.size()) + " connections");
}
