#include "ServerManager.h"
#include <iostream>
#include <WS2tcpip.h>

// Global Winsock initialization flag (for simplicity, assuming one-time init)
static bool g_winsock_initialized = false;

ServerManager::ServerManager()
	: listen_socket_(INVALID_SOCKET),
	  accept_context_(IO_TYPE::IO_ACCEPT)
{
	// Initialize accept buffer size
	accept_buffer_.resize((sizeof(SOCKADDR_IN) + 16) * 2);

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

ServerManager::~ServerManager() 
{
	StopServer();

	// Cleanup Winsock if this is the last user (simplistic, consider ref counting)
	if (g_winsock_initialized)
	{
		WSACleanup();
		g_winsock_initialized = false;
	}
}

bool ServerManager::StartServer(std::string const& _bind_ip, uint16 _port, int _worker_cnt)
{
	if (!g_winsock_initialized)
	{
		std::cerr << "Winsock not initialized." << std::endl;
		return false;
	}

	// 1. Create listening socket
	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listen_socket_ == INVALID_SOCKET)
	{
		std::cerr << "WSASocket failed: " << WSAGetLastError() << std::endl;
		return false;
	}

	// 2. Bind socket
	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	InetPtonA(AF_INET, _bind_ip.c_str(), &server_addr.sin_addr);
	server_addr.sin_port = htons(_port);

	if (bind(listen_socket_, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
	{
		std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	// 3. Listen
	if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	// 4. Start IOCP worker threads first
	if (!iocp_manager_.Start(_worker_cnt))
	{
		std::cerr << "IOCP Manager failed to start." << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	// 5. Associate listening socket with IOCP
	if (CreateIoCompletionPort((HANDLE)listen_socket_, iocp_manager_.GetIocpHandle(), (ULONG_PTR)0, 0) == NULL)
	{
		std::cerr << "CreateIoCompletionPort for listen_socket failed: " << GetLastError() << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	// 6. Load AcceptEx and GetAcceptExSockaddrs function pointers
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	DWORD bytes = 0;

	if (WSAIoctl(listen_socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &lpfnAcceptEx_, sizeof(lpfnAcceptEx_), &bytes, NULL, NULL) == SOCKET_ERROR)
	{
		std::cerr << "WSAIoctl(AcceptEx) failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	if (WSAIoctl(listen_socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs), &lpfnGetAcceptExSockaddrs_, sizeof(lpfnGetAcceptExSockaddrs_), &bytes, NULL, NULL) == SOCKET_ERROR)
	{
		std::cerr << "WSAIoctl(GetAcceptExSockaddrs) failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket_);
		return false;
	}

	// 7. Prepare accept buffer (size for local and remote addresses + data)
	accept_buffer_.resize(sizeof(SOCKADDR_IN) * 2 + 16);

	// 8. Initiate first AsyncAccept calls
	AsyncAccept();

	std::cout << "Server started on " << _bind_ip << ":" << _port << std::endl;
	return true;
}

void ServerManager::StopServer()
{
	if (listen_socket_ != INVALID_SOCKET)
	{
		closesocket(listen_socket_);
		listen_socket_ = INVALID_SOCKET;
	}
	iocp_manager_.Stop();
}

void ServerManager::AsyncAccept()
{
	// Create a new socket for the accepted connection
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (client_socket == INVALID_SOCKET)
	{
		std::cerr << "WSASocket for client failed: " << WSAGetLastError() << std::endl;
		return;
	}

	// Associate the new client socket with IOCP
	if (CreateIoCompletionPort((HANDLE)client_socket, iocp_manager_.GetIocpHandle(), (ULONG_PTR)client_socket, 0) == NULL)
	{
		std::cerr << "CreateIoCompletionPort for client_socket failed: " << GetLastError() << std::endl;
		closesocket(client_socket);
		return;
	}

	// Initiate AcceptEx
	DWORD bytesReceived = 0;
	BOOL ret = lpfnAcceptEx_(
		listen_socket_,
		client_socket,
		accept_buffer_.data(),
		0, // Receive 0 bytes initially, just establish connection
		sizeof(SOCKADDR_IN) + 16,
		sizeof(SOCKADDR_IN) + 16,
		&bytesReceived,
		&accept_context_.overlapped
	);

	if (ret == FALSE && (WSAGetLastError() != WSA_IO_PENDING))
	{
		std::cerr << "AcceptEx failed: " << WSAGetLastError() << std::endl;
		closesocket(client_socket);
		// Re-post another accept if this one failed synchronously
		AsyncAccept();
	}

	// The completion will be handled by IocpManager's worker thread
	// and then dispatched to a handler (e.g., in ServerManager or Session)
}


