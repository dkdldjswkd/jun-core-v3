#pragma once

#include "NetworkCore.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

// Network initialization helper
class NetworkInitializer
{
public:
	static NetworkError Initialize();
	static void Cleanup();
	static bool IsInitialized();

private:
	static std::atomic<bool> is_initialized_;
	static std::mutex init_mutex_;
};

// Network utility functions
class NetworkUtils
{
public:
	static std::string GetErrorMessage(NetworkError error);
	static std::string SocketErrorToString(int error_code);
	static std::string GetLocalIPAddress();
	static bool IsValidIPAddress(const std::string& address);
	static bool IsValidPort(Port port);

	// Address resolution
	static std::vector<std::string> ResolveHostname(const std::string& hostname);
	static std::string GetSocketAddress(SOCKET socket);
	static Port GetSocketPort(SOCKET socket);
};