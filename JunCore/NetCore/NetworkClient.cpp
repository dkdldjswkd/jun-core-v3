#include "NetworkClient.h"
#include "Logger.h"
#include <iostream>
#include <WS2tcpip.h>

//==============================================================================
// NetworkClient Implementation  
//==============================================================================

NetworkClient::NetworkClient()
	: iocp_handle_(nullptr),
	is_running_(false),
	event_handler_(nullptr),
	connection_id_counter_(1)
{
}

NetworkClient::~NetworkClient()
{
	Disconnect();
}

std::future<NetworkError> NetworkClient::ConnectAsync(const std::string& address, Port port,
	IClientEventHandler* handler)
{
	return std::async(std::launch::async, [this, address, port, handler]()
		{
			return Connect(address, port, handler, kDefaultConnectionTimeoutMs);
		});
}

NetworkError NetworkClient::Connect(const std::string& address, Port port,
	IClientEventHandler* handler, int timeout_ms)
{
	std::lock_guard<std::mutex> lock(state_mutex_);

	if (IsConnected())
	{
		Logger::Warning("Client is already connected");
		return NetworkError::kInitializationFailed;
	}

	if (!handler)
	{
		Logger::Error("Event handler cannot be null");
		return NetworkError::kInvalidParameter;
	}

	event_handler_ = handler;

	// Notify connection attempt
	event_handler_->OnConnectionAttempt(address, port);

	// Initialize network components
	NetworkError result = InitializeWinsock();
	if (result != NetworkError::kSuccess)
	{
		return result;
	}

	result = CreateClientSocket();
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

	result = ConnectInternal(address, port);
	if (result != NetworkError::kSuccess)
	{
		CleanupNetwork();
		return result;
	}

	is_running_ = true;
	StartReceiveLoop();

	// Notify successful connection
	event_handler_->OnConnectionEstablished();
	event_handler_->OnConnected(connection_);

	Logger::Info("Connected to " + address + ":" + std::to_string(port));
	return NetworkError::kSuccess;
}

void NetworkClient::Disconnect()
{
	std::lock_guard<std::mutex> lock(state_mutex_);

	if (!is_running_)
	{
		return;
	}

	Logger::Info("Disconnecting client...");
	is_running_ = false;

	if (connection_)
	{
		connection_->Disconnect();
		if (event_handler_)
		{
			event_handler_->OnConnectionLost();
			event_handler_->OnDisconnected(connection_);
		}
	}

	StopReceiveLoop();
	CleanupNetwork();

	connection_.reset();

	Logger::Info("Client disconnected");
}

bool NetworkClient::IsConnected() const
{
	return connection_ && connection_->IsValid() && is_running_;
}

ConnectionState NetworkClient::GetState() const
{
	if (!connection_)
	{
		return ConnectionState::kDisconnected;
	}
	return connection_->GetState();
}

NetworkError NetworkClient::Send(const char* data, size_t length)
{
	if (!IsConnected())
	{
		return NetworkError::kConnectionFailed;
	}
	return connection_->Send(data, length);
}

NetworkError NetworkClient::Send(const std::string& message)
{
	return Send(message.c_str(), message.length());
}

NetworkError NetworkClient::Send(const ByteBuffer& buffer)
{
	return Send(buffer.data(), buffer.size());
}

std::shared_ptr<IConnection> NetworkClient::GetConnection()
{
	return connection_;
}

NetworkError NetworkClient::InitializeWinsock()
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

NetworkError NetworkClient::CreateClientSocket()
{
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET)
	{
		Logger::Error("socket() failed with error: " + std::to_string(WSAGetLastError()));
		return NetworkError::kInitializationFailed;
	}

	// Create connection object
	ConnectionId id = connection_id_counter_++;
	connection_ = std::make_shared<NetworkConnection>(client_socket, id);
	connection_->SetState(ConnectionState::kDisconnected);

	return NetworkError::kSuccess;
}

NetworkError NetworkClient::CreateIocpHandle()
{
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (!iocp_handle_)
	{
		Logger::Error("CreateIoCompletionPort failed with error: " + std::to_string(GetLastError()));
		return NetworkError::kInitializationFailed;
	}

	// Associate socket with IOCP
	if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(connection_->GetSocket()),
		iocp_handle_,
		reinterpret_cast<ULONG_PTR>(connection_.get()),
		0) == nullptr)
	{
		Logger::Error("CreateIoCompletionPort failed for client socket");
		return NetworkError::kInitializationFailed;
	}

	return NetworkError::kSuccess;
}

void NetworkClient::CleanupNetwork()
{
	if (iocp_handle_)
	{
		CloseHandle(iocp_handle_);
		iocp_handle_ = nullptr;
	}

	WSACleanup();
}

NetworkError NetworkClient::ConnectInternal(const std::string& address, Port port)
{
	sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	// Convert address string to binary format
	if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0)
	{
		// Try to resolve hostname
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* result = nullptr;
		if (getaddrinfo(address.c_str(), nullptr, &hints, &result) != 0)
		{
			Logger::Error("Failed to resolve hostname: " + address);
			return NetworkError::kConnectionFailed;
		}

		sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(result->ai_addr);
		server_addr.sin_addr = addr_in->sin_addr;
		freeaddrinfo(result);
	}

	connection_->SetState(ConnectionState::kConnecting);

	if (connect(connection_->GetSocket(),
		reinterpret_cast<sockaddr*>(&server_addr),
		sizeof(server_addr)) == SOCKET_ERROR)
	{
		Logger::Error("connect() failed with error: " + std::to_string(WSAGetLastError()));
		connection_->SetState(ConnectionState::kDisconnected);
		return NetworkError::kConnectionFailed;
	}

	connection_->SetState(ConnectionState::kConnected);
	return NetworkError::kSuccess;
}

void NetworkClient::StartReceiveLoop()
{
	worker_thread_ = std::thread(&NetworkClient::WorkerThreadProc, this);
	Logger::Debug("Client worker thread started");
}

void NetworkClient::StopReceiveLoop()
{
	if (iocp_handle_)
	{
		PostQueuedCompletionStatus(iocp_handle_, 0, 0, nullptr);
	}

	if (worker_thread_.joinable())
	{
		worker_thread_.join();
		Logger::Debug("Client worker thread stopped");
	}
}

void NetworkClient::WorkerThreadProc()
{
	// Start initial receive operation
	HandleReceive();

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
				HandleDisconnection();
			}
			break;
		}

		// Check for shutdown signal
		if (!overlapped)
		{
			break;
		}

		// Handle disconnection
		if (bytes_transferred == 0)
		{
			HandleDisconnection();
			break;
		}

		// Handle received data
		if (event_handler_ && connection_)
		{
			event_handler_->OnDataReceived(connection_, connection_->GetReceiveBuffer(), bytes_transferred);
		}

		// Continue receiving
		HandleReceive();
	}
}

void NetworkClient::HandleReceive()
{
	if (!connection_ || !connection_->IsValid())
	{
		return;
	}

	connection_->ResetReceiveContext();

	WSABUF receive_buffer = {
	  static_cast<ULONG>(connection_->GetReceiveBufferSize()),
	  connection_->GetReceiveBuffer()
	};

	DWORD flags = 0;
	DWORD bytes_received = 0;

	int result = WSARecv(connection_->GetSocket(),
		&receive_buffer,
		1,
		&bytes_received,
		&flags,
		&connection_->GetReceiveContext()->overlapped,
		nullptr);

	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		HandleDisconnection();
	}
}

void NetworkClient::HandleDisconnection()
{
	if (connection_)
	{
		connection_->SetState(ConnectionState::kDisconnected);
		if (event_handler_)
		{
			event_handler_->OnConnectionLost();
			event_handler_->OnDisconnected(connection_);
		}
	}
	is_running_ = false;
}
