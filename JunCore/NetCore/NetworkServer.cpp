#include "NetworkServer.h"
#include "Logger.h"
#include <iostream>


//==============================================================================
// NetworkServer Implementation
//==============================================================================

NetworkServer::NetworkServer(std::unique_ptr<IConnectionManager> connection_manager)
	: listen_socket_(INVALID_SOCKET),
	iocp_handle_(nullptr),
	is_running_(false),
	connection_manager_(connection_manager ? std::move(connection_manager)
		: std::make_unique<ConnectionManager>()),
	event_handler_(nullptr),
	next_connection_id_(1)
{
}

NetworkServer::~NetworkServer()
{
	Stop();
}

NetworkError NetworkServer::Start(Port port, IServerEventHandler* handler, int worker_thread_count)
{
	if (is_running_)
	{
		Logger::Warning("Server is already running");
		return NetworkError::kInitializationFailed;
	}

	if (!handler)
	{
		Logger::Error("Event handler cannot be null");
		return NetworkError::kInvalidParameter;
	}

	event_handler_ = handler;

	// Initialize network components
	NetworkError result = InitializeWinsock();
	if (result != NetworkError::kSuccess)
	{
		return result;
	}

	result = CreateListenSocket(port);
	if (result != NetworkError::kSuccess)
	{
		CleanupNetwork();
		return result;
	}

	result = CreateIocpHandle();
	if (result != NetworkError::kSuccess)
	{
		CleanupNetwork();
		return result;
	}

	is_running_ = true;

	// Start worker and accept threads
	StartWorkerThreads(worker_thread_count);
	StartAcceptThread();

	Logger::Info("NetworkServer started on port " + std::to_string(port));
	return NetworkError::kSuccess;
}

void NetworkServer::Stop()
{
	if (!is_running_)
	{
		return;
	}

	Logger::Info("Stopping NetworkServer...");
	is_running_ = false;

	// Close listen socket to stop accepting new connections
	if (listen_socket_ != INVALID_SOCKET)
	{
		closesocket(listen_socket_);
		listen_socket_ = INVALID_SOCKET;
	}

	// Signal all worker threads to exit
	if (iocp_handle_)
	{
		for (size_t i = 0; i < worker_threads_.size(); ++i)
		{
			PostQueuedCompletionStatus(iocp_handle_, 0, 0, nullptr);
		}
	}

	StopAllThreads();
	connection_manager_->ClearConnections();
	CleanupNetwork();

	Logger::Info("NetworkServer stopped");
}

bool NetworkServer::IsRunning() const
{
	return is_running_;
}

size_t NetworkServer::GetClientCount() const
{
	return connection_manager_->GetConnectionCount();
}

void NetworkServer::BroadcastToAllClients(const char* data, size_t length)
{
	connection_manager_->BroadcastToAll(data, length);
}

void NetworkServer::BroadcastToAllClients(const std::string& message)
{
	BroadcastToAllClients(message.c_str(), message.length());
}

std::shared_ptr<IConnection> NetworkServer::FindClient(ConnectionId id)
{
	return connection_manager_->FindConnection(id);
}

void NetworkServer::DisconnectClient(ConnectionId id)
{
	auto connection = connection_manager_->FindConnection(id);
	if (connection)
	{
		connection->Disconnect();
		connection_manager_->RemoveConnection(id);
	}
}

NetworkError NetworkServer::InitializeWinsock()
{
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0)
	{
		Logger::Error("WSAStartup failed with error: " + std::to_string(result));
		return NetworkError::kInitializationFailed;
	}
	return NetworkError::kSuccess;
}

NetworkError NetworkServer::CreateListenSocket(Port port)
{
	listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket_ == INVALID_SOCKET)
	{
		Logger::Error("socket() failed with error: " + std::to_string(WSAGetLastError()));
		return NetworkError::kInitializationFailed;
	}

	// Enable socket reuse
	BOOL reuse_addr = TRUE;
	setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
		reinterpret_cast<const char*>(&reuse_addr), sizeof(reuse_addr));

	sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr),
		sizeof(server_addr)) == SOCKET_ERROR)
	{
		Logger::Error("bind() failed with error: " + std::to_string(WSAGetLastError()));
		return NetworkError::kInitializationFailed;
	}

	if (listen(listen_socket_, kMaxPendingConnections) == SOCKET_ERROR)
	{
		Logger::Error("listen() failed with error: " + std::to_string(WSAGetLastError()));
		return NetworkError::kInitializationFailed;
	}

	return NetworkError::kSuccess;
}

NetworkError NetworkServer::CreateIocpHandle()
{
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (!iocp_handle_)
	{
		Logger::Error("CreateIoCompletionPort failed with error: " + std::to_string(GetLastError()));
		return NetworkError::kInitializationFailed;
	}
	return NetworkError::kSuccess;
}

void NetworkServer::CleanupNetwork()
{
	if (iocp_handle_)
	{
		CloseHandle(iocp_handle_);
		iocp_handle_ = nullptr;
	}

	if (listen_socket_ != INVALID_SOCKET)
	{
		closesocket(listen_socket_);
		listen_socket_ = INVALID_SOCKET;
	}

	WSACleanup();
}

void NetworkServer::StartWorkerThreads(int count)
{
	if (count <= 0)
	{
		count = std::thread::hardware_concurrency();
		if (count <= 0) count = 2;  // Fallback to 2 threads
	}

	worker_threads_.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		worker_threads_.emplace_back(&NetworkServer::WorkerThreadProc, this);
	}

	Logger::Info("Started " + std::to_string(count) + " worker threads");
}

void NetworkServer::StartAcceptThread()
{
	accept_thread_ = std::thread(&NetworkServer::AcceptThreadProc, this);
	Logger::Debug("Accept thread started");
}

void NetworkServer::StopAllThreads()
{
	// Wait for accept thread
	if (accept_thread_.joinable())
	{
		accept_thread_.join();
		Logger::Debug("Accept thread stopped");
	}

	// Wait for worker threads
	for (auto& thread : worker_threads_)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}

	Logger::Debug("All worker threads stopped");
	worker_threads_.clear();
}

void NetworkServer::AcceptThreadProc()
{
	Logger::Debug("Accept thread started");

	while (is_running_)
	{
		sockaddr_in client_addr = {};
		int addr_len = sizeof(client_addr);

		SOCKET client_socket = accept(listen_socket_,
			reinterpret_cast<sockaddr*>(&client_addr),
			&addr_len);

		if (client_socket == INVALID_SOCKET)
		{
			if (is_running_)
			{
				Logger::Warning("accept() failed with error: " + std::to_string(WSAGetLastError()));
			}
			continue;
		}

		// Create new connection
		ConnectionId connection_id = next_connection_id_++;
		auto connection = std::make_shared<NetworkConnection>(client_socket, connection_id);

		// Associate socket with IOCP
		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket),
			iocp_handle_,
			reinterpret_cast<ULONG_PTR>(connection.get()),
			0) == nullptr)
		{
			Logger::Error("CreateIoCompletionPort failed for client socket");
			connection->Disconnect();
			continue;
		}

		connection_manager_->AddConnection(connection);

		// Notify event handler
		if (event_handler_)
		{
			event_handler_->OnClientAccepted(connection);
			event_handler_->OnConnected(connection);
		}

		// Start receiving data
		HandleReceive(connection);
	}

	Logger::Debug("Accept thread ending");
}

void NetworkServer::WorkerThreadProc()
{
	while (is_running_)
	{
		DWORD bytes_transferred = 0;
		ULONG_PTR completion_key = 0;
		LPOVERLAPPED overlapped = nullptr;

		BOOL result = GetQueuedCompletionStatus(iocp_handle_,
			&bytes_transferred,
			&completion_key,
			&overlapped,
			kIocpTimeoutMs);

		if (!result)
		{
			DWORD error = GetLastError();
			if (error == WAIT_TIMEOUT)
			{
				continue;  // Normal timeout, check running flag
			}
			if (is_running_)
			{
				Logger::Warning("GetQueuedCompletionStatus failed with error: " + std::to_string(error));
			}
			continue;
		}

		// Check for shutdown signal
		if (!overlapped)
		{
			break;
		}

		// Get IOContext from overlapped
		IOContext* io_context = reinterpret_cast<IOContext*>(overlapped);
		if (!io_context)
		{
			continue;
		}

		// Get connection from IOContext
		NetworkConnection* connection_ptr = static_cast<NetworkConnection*>(io_context->connection);
		if (!connection_ptr)
		{
			continue;
		}

		// Find connection in manager to get shared_ptr
		auto connection = connection_manager_->FindConnection(connection_ptr->GetId());
		if (!connection)
		{
			continue;
		}

		// Handle disconnection
		if (bytes_transferred == 0)
		{
			CloseConnection(connection);
			continue;
		}

		// Handle based on operation type
		switch (io_context->operation)
		{
		case IOOperation::kReceive:
			HandleReceiveCompletion(connection, bytes_transferred);
			break;
		case IOOperation::kSend:
			HandleSendCompletion(connection, bytes_transferred);
			break;
		default:
			Logger::Warning("Unknown IOCP operation type");
			break;
		}
		{
			event_handler_->OnDataReceived(connection, connection->GetReceiveBuffer(), bytes_transferred);
		}

		// Continue receiving
		HandleReceive(connection);
	}
}

void NetworkServer::HandleReceive(std::shared_ptr<NetworkConnection> connection)
{
	if (!connection || !connection->IsValid())
	{
		return;
	}

	connection->ResetReceiveContext();

	WSABUF receive_buffer = {
	  static_cast<ULONG>(connection->GetReceiveBufferSize()),
	  connection->GetReceiveBuffer()
	};

	DWORD flags = 0;
	DWORD bytes_received = 0;

	int result = WSARecv(connection->GetSocket(),
		&receive_buffer,
		1,
		&bytes_received,
		&flags,
		&connection->GetReceiveContext()->overlapped,
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		CloseConnection(connection);
	}
}

void NetworkServer::CloseConnection(std::shared_ptr<NetworkConnection> connection)
{
	if (!connection)
	{
		return;
	}

	connection_manager_->RemoveConnection(connection->GetId());

	if (event_handler_)
	{
		event_handler_->OnDisconnected(connection);
	}

	connection->Disconnect();
}


void NetworkServer::HandleReceiveCompletion(std::shared_ptr<NetworkConnection> connection, DWORD bytes_transferred)
{
	if (!connection || !event_handler_)
	{
		return;
	}

	// Process received data
	event_handler_->OnDataReceived(connection, connection->GetReceiveBuffer(), bytes_transferred);

	// Reset receive context for next operation
	connection->ResetReceiveContext();

	// Post next receive operation
	HandleReceive(connection);
}

void NetworkServer::HandleSendCompletion(std::shared_ptr<NetworkConnection> connection, DWORD bytes_transferred)
{
	if (!connection)
	{
		return;
	}

	// Reset send context for next operation
	connection->ResetSendContext();

	// Log successful send (optional)
	Logger::Debug("Send completed for connection " + std::to_string(connection->GetId()) + 
	              ", bytes: " + std::to_string(bytes_transferred));

	// Note: If you need to track pending sends or implement send queue,
	// this is where you would handle it
}
