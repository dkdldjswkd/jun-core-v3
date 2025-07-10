#pragma once
#include "NetworkConnection.h"

//==============================================================================
// Server Interface
//==============================================================================

class INetworkServer
{
public:
	virtual ~INetworkServer() = default;

	virtual NetworkError Start(Port port, IServerEventHandler* handler,
		int worker_thread_count = 0) = 0;
	virtual void Stop() = 0;
	virtual bool IsRunning() const = 0;
	virtual size_t GetClientCount() const = 0;
	virtual void BroadcastToAllClients(const char* data, size_t length) = 0;
	virtual void BroadcastToAllClients(const std::string& message) = 0;
	virtual std::shared_ptr<IConnection> FindClient(ConnectionId id) = 0;
	virtual void DisconnectClient(ConnectionId id) = 0;
};

//==============================================================================
// IOCP-based Network Server Implementation
//==============================================================================

class NetworkServer : public INetworkServer
{
public:
	explicit NetworkServer(std::unique_ptr<IConnectionManager> connection_manager = nullptr);
	~NetworkServer() override;

	// INetworkServer interface
	NetworkError Start(Port port, IServerEventHandler* handler,
		int worker_thread_count = 0) override;
	void Stop() override;
	bool IsRunning() const override;
	size_t GetClientCount() const override;
	void BroadcastToAllClients(const char* data, size_t length) override;
	void BroadcastToAllClients(const std::string& message) override;
	std::shared_ptr<IConnection> FindClient(ConnectionId id) override;
	void DisconnectClient(ConnectionId id) override;

private:
	// Network initialization
	NetworkError InitializeWinsock();
	NetworkError CreateListenSocket(Port port);
	NetworkError CreateIocpHandle();
	void CleanupNetwork();

	// Thread management
	void StartWorkerThreads(int count);
	void StartAcceptThread();
	void StopAllThreads();

	// Core network operations
	void WorkerThreadProc();
	void AcceptThreadProc();
	void HandleReceive(std::shared_ptr<NetworkConnection> connection);
	void HandleReceiveCompletion(std::shared_ptr<NetworkConnection> connection, DWORD bytes_transferred);
	void HandleSendCompletion(std::shared_ptr<NetworkConnection> connection, DWORD bytes_transferred);
	void CloseConnection(std::shared_ptr<NetworkConnection> connection);

	// Member variables
	SOCKET listen_socket_;
	HANDLE iocp_handle_;
	std::vector<std::thread> worker_threads_;
	std::thread accept_thread_;
	std::atomic<bool> is_running_;
	std::unique_ptr<IConnectionManager> connection_manager_;
	IServerEventHandler* event_handler_;
	std::atomic<ConnectionId> next_connection_id_;

	// Configuration constants
	static constexpr int kDefaultWorkerThreadCount = 0;
	static constexpr int kMaxPendingConnections = SOMAXCONN;
	static constexpr DWORD kIocpTimeoutMs = 1000;
};