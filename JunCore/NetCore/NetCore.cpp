#include "NetCore.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
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

//==============================================================================
// ConsoleLogger Implementation
//==============================================================================

void ConsoleLogger::Log(LogLevel level, const std::string& message)
{
	if (level < min_level_)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(log_mutex_);

	// Get current time
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::ostringstream oss;

	// Use localtime_s for safety
	std::tm local_tm;
	if (localtime_s(&local_tm, &time_t) == 0)
	{
		oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
	}
	else
	{
		oss << "TIME_ERROR";
	}

	oss << "." << std::setfill('0') << std::setw(3) << ms.count();

	std::string level_str;
	switch (level)
	{
	case LogLevel::kDebug:
		level_str = "[DEBUG]";
		break;
	case LogLevel::kInfo:
		level_str = "[INFO]";
		break;
	case LogLevel::kWarning:
		level_str = "[WARN]";
		break;
	case LogLevel::kError:
		level_str = "[ERROR]";
		break;
	}

	std::cout << oss.str() << " " << level_str << " " << message << std::endl;
}

void ConsoleLogger::SetMinLevel(LogLevel min_level)
{
	min_level_ = min_level;
}

//==============================================================================
// Logger Implementation
//==============================================================================

std::shared_ptr<ILogger> Logger::logger_ = std::make_shared<ConsoleLogger>();
std::mutex Logger::logger_mutex_;

void Logger::SetLogger(std::shared_ptr<ILogger> logger)
{
	std::lock_guard<std::mutex> lock(logger_mutex_);
	logger_ = logger ? logger : std::make_shared<ConsoleLogger>();
}

void Logger::Log(LogLevel level, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logger_mutex_);
	if (logger_)
	{
		logger_->Log(level, message);
	}
}

void Logger::Debug(const std::string& message)
{
	Log(LogLevel::kDebug, message);
}

void Logger::Info(const std::string& message)
{
	Log(LogLevel::kInfo, message);
}

void Logger::Warning(const std::string& message)
{
	Log(LogLevel::kWarning, message);
}

void Logger::Error(const std::string& message)
{
	Log(LogLevel::kError, message);
}

//==============================================================================
// NetworkFactory Implementation
//==============================================================================

std::unique_ptr<INetworkServer> NetworkFactory::CreateServer(
	std::unique_ptr<IConnectionManager> connection_manager)
{
	return std::make_unique<NetworkServer>(std::move(connection_manager));
}

std::unique_ptr<INetworkClient> NetworkFactory::CreateClient()
{
	return std::make_unique<NetworkClient>();
}

std::unique_ptr<IConnectionManager> NetworkFactory::CreateConnectionManager()
{
	return std::make_unique<ConnectionManager>();
}
