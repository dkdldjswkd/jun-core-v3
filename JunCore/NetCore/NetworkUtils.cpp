#include "NetworkUtils.h"
#include <iostream>
#include <sstream>
#include <WS2tcpip.h>

std::atomic<bool> NetworkInitializer::is_initialized_{ false };
std::mutex NetworkInitializer::init_mutex_;

NetworkError NetworkInitializer::Initialize()
{
	std::lock_guard<std::mutex> lock(init_mutex_);

	if (is_initialized_)
	{
		return NetworkError::kSuccess;
	}

	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0)
	{
		return NetworkError::kInitializationFailed;
	}

	is_initialized_ = true;
	return NetworkError::kSuccess;
}

void NetworkInitializer::Cleanup()
{
	std::lock_guard<std::mutex> lock(init_mutex_);

	if (is_initialized_)
	{
		WSACleanup();
		is_initialized_ = false;
	}
}

bool NetworkInitializer::IsInitialized()
{
	return is_initialized_;
}

//==============================================================================
// NetworkUtils Implementation
//==============================================================================

std::string NetworkUtils::GetErrorMessage(NetworkError error)
{
	switch (error)
	{
	case NetworkError::kSuccess:
		return "Success";
	case NetworkError::kInitializationFailed:
		return "Network initialization failed";
	case NetworkError::kConnectionFailed:
		return "Connection failed";
	case NetworkError::kSendFailed:
		return "Send operation failed";
	case NetworkError::kReceiveFailed:
		return "Receive operation failed";
	case NetworkError::kInvalidParameter:
		return "Invalid parameter";
	case NetworkError::kTimeout:
		return "Operation timeout";
	case NetworkError::kUnknownError:
	default:
		return "Unknown error";
	}
}

std::string NetworkUtils::SocketErrorToString(int error_code)
{
	std::ostringstream oss;
	oss << "Socket error " << error_code;

	switch (error_code)
	{
	case WSAEACCES:
		oss << " (Access denied)";
		break;
	case WSAEADDRINUSE:
		oss << " (Address already in use)";
		break;
	case WSAEADDRNOTAVAIL:
		oss << " (Address not available)";
		break;
	case WSAECONNREFUSED:
		oss << " (Connection refused)";
		break;
	case WSAECONNRESET:
		oss << " (Connection reset by peer)";
		break;
	case WSAEHOSTUNREACH:
		oss << " (Host unreachable)";
		break;
	case WSAENETDOWN:
		oss << " (Network is down)";
		break;
	case WSAENETUNREACH:
		oss << " (Network unreachable)";
		break;
	case WSAETIMEDOUT:
		oss << " (Connection timed out)";
		break;
	default:
		break;
	}

	return oss.str();
}

std::string NetworkUtils::GetLocalIPAddress()
{
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
	{
		return "127.0.0.1";
	}

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* result = nullptr;
	if (getaddrinfo(hostname, nullptr, &hints, &result) != 0)
	{
		return "127.0.0.1";
	}

	char ip_str[INET_ADDRSTRLEN];
	sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(result->ai_addr);
	inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN);

	freeaddrinfo(result);
	return std::string(ip_str);
}

bool NetworkUtils::IsValidIPAddress(const std::string& address)
{
	sockaddr_in sa;
	return inet_pton(AF_INET, address.c_str(), &(sa.sin_addr)) == 1;
}

bool NetworkUtils::IsValidPort(Port port)
{
	return port > 0 && port <= 65535;
}

std::vector<std::string> NetworkUtils::ResolveHostname(const std::string& hostname)
{
	std::vector<std::string> addresses;

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* result = nullptr;
	if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0)
	{
		return addresses;
	}

	for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
	{
		sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
		char ip_str[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN))
		{
			addresses.emplace_back(ip_str);
		}
	}

	freeaddrinfo(result);
	return addresses;
}

std::string NetworkUtils::GetSocketAddress(SOCKET socket)
{
	sockaddr_in addr;
	int addr_len = sizeof(addr);

	if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0)
	{
		char ip_str[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN))
		{
			return std::string(ip_str);
		}
	}

	return "Unknown";
}

Port NetworkUtils::GetSocketPort(SOCKET socket)
{
	sockaddr_in addr;
	int addr_len = sizeof(addr);

	if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0)
	{
		return ntohs(addr.sin_port);
	}

	return 0;
}