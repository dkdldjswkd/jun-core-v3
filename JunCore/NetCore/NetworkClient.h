#pragma once

#include "NetworkConnection.h"
#include <future>


//==============================================================================
// Client Interface
//==============================================================================

class INetworkClient
{
public:
	virtual ~INetworkClient() = default;

	virtual std::future<NetworkError> ConnectAsync(const std::string& address, Port port,
		IClientEventHandler* handler) = 0;
	virtual NetworkError Connect(const std::string& address, Port port,
		IClientEventHandler* handler,
		int timeout_ms = 5000) = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() const = 0;
	virtual ConnectionState GetState() const = 0;
	virtual NetworkError Send(const char* data, size_t length) = 0;
	virtual NetworkError Send(const std::string& message) = 0;
	virtual NetworkError Send(const ByteBuffer& buffer) = 0;
	virtual std::shared_ptr<IConnection> GetConnection() = 0;
};

//==============================================================================
// IOCP-based Network Client Implementation
//==============================================================================

class NetworkClient : public INetworkClient
{
public:
	NetworkClient();
	~NetworkClient() override;

	// INetworkClient interface
	std::future<NetworkError> ConnectAsync(const std::string& address, Port port,
		IClientEventHandler* handler) override;
	NetworkError Connect(const std::string& address, Port port,
		IClientEventHandler* handler,
		int timeout_ms = 5000) override;
	void Disconnect() override;
	bool IsConnected() const override;
	ConnectionState GetState() const override;
	NetworkError Send(const char* data, size_t length) override;
	NetworkError Send(const std::string& message) override;
	NetworkError Send(const ByteBuffer& buffer) override;
	std::shared_ptr<IConnection> GetConnection() override;

private:
	// Network initialization
	NetworkError InitializeWinsock();
	NetworkError CreateClientSocket();
	NetworkError CreateIocpHandle();
	void CleanupNetwork();

	// Connection management
	NetworkError ConnectInternal(const std::string& address, Port port);
	void StartReceiveLoop();
	void StopReceiveLoop();

	// Core network operations
	void WorkerThreadProc();
	void HandleReceive();
	void HandleDisconnection();

	// Member variables
	std::shared_ptr<NetworkConnection> connection_;
	HANDLE iocp_handle_;
	std::thread worker_thread_;
	std::atomic<bool> is_running_;
	IClientEventHandler* event_handler_;
	std::atomic<ConnectionId> connection_id_counter_;

	// Synchronization
	mutable std::mutex state_mutex_;

	// Configuration constants
	static constexpr DWORD kIocpTimeoutMs = 1000;
	static constexpr int kDefaultConnectionTimeoutMs = 5000;
};
