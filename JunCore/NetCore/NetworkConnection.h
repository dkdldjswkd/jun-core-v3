#pragma once

#include "NetworkCore.h"
#include <vector>
#include <thread>
#include <unordered_map>

//==============================================================================
// IOCP Operation Types
//==============================================================================

enum class IOOperation
{
	kReceive,
	kSend,
	kConnect,
	kDisconnect
};

// Extended OVERLAPPED structure to identify operation type
struct IOContext
{
	OVERLAPPED overlapped;
	IOOperation operation;
	void* connection;  // Pointer back to the connection
	
	IOContext(IOOperation op, void* conn = nullptr) 
		: operation(op), connection(conn)
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
	}
	
	void Reset()
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		// operation and connection remain the same
	}
};

//==============================================================================
// Connection Implementation
//==============================================================================

class NetworkConnection : public IConnection,
	public std::enable_shared_from_this<NetworkConnection>
{
public:
	explicit NetworkConnection(SOCKET socket, ConnectionId id);
	~NetworkConnection() override;

	// IConnection interface implementation
	ConnectionId GetId() const override;
	SOCKET GetSocket() const override;
	ConnectionState GetState() const override;
	bool IsValid() const override;

	NetworkError Send(const char* data, size_t length) override;
	NetworkError Send(const std::string& message) override;
	NetworkError Send(const ByteBuffer& buffer) override;

	void Disconnect() override;
	std::string GetRemoteAddress() const override;
	Port GetRemotePort() const override;

	// Internal methods for IOCP operations
	char* GetReceiveBuffer()
	{
		return receive_buffer_;
	}

	size_t GetReceiveBufferSize() const
	{
		return kReceiveBufferSize;
	}

	IOContext* GetReceiveContext()
	{
		return &recv_context_;
	}

	IOContext* GetSendContext()
	{
		return &send_context_;
	}

	void ResetReceiveContext();
	void ResetSendContext();
	void SetState(ConnectionState state);

	// IOCP 관련 추가 메서드  
	void ResetOverlapped();

private:
	static constexpr size_t kReceiveBufferSize = 8192;

	SOCKET socket_;
	ConnectionId connection_id_;
	std::atomic<ConnectionState> state_;
	char receive_buffer_[kReceiveBufferSize];
	IOContext recv_context_;
	IOContext send_context_;
	mutable std::mutex socket_mutex_;

	// Connection info cache
	mutable std::string remote_address_;
	mutable Port remote_port_;
	mutable bool address_cached_;

	void CacheRemoteAddress() const;
	NetworkError SendInternal(const char* data, size_t length);
};

//==============================================================================
// Connection Manager Interface
//==============================================================================

class IConnectionManager
{
public:
	virtual ~IConnectionManager() = default;

	virtual void AddConnection(std::shared_ptr<NetworkConnection> connection) = 0;
	virtual void RemoveConnection(ConnectionId id) = 0;
	virtual std::shared_ptr<NetworkConnection> FindConnection(ConnectionId id) = 0;
	virtual std::vector<std::shared_ptr<NetworkConnection>> GetAllConnections() = 0;
	virtual void ClearConnections() = 0;
	virtual size_t GetConnectionCount() const = 0;
	virtual void BroadcastToAll(const char* data, size_t length) = 0;
};

// Concrete connection manager implementation
class ConnectionManager : public IConnectionManager
{
public:
	ConnectionManager() = default;
	~ConnectionManager() = default;

	void AddConnection(std::shared_ptr<NetworkConnection> connection) override;
	void RemoveConnection(ConnectionId id) override;
	std::shared_ptr<NetworkConnection> FindConnection(ConnectionId id) override;
	std::vector<std::shared_ptr<NetworkConnection>> GetAllConnections() override;
	void ClearConnections() override;
	size_t GetConnectionCount() const override;
	void BroadcastToAll(const char* data, size_t length) override;

private:
	mutable std::mutex connections_mutex_;
	std::unordered_map<ConnectionId, std::shared_ptr<NetworkConnection>> connections_;
};