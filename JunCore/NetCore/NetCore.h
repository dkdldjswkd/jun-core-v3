#pragma once

#include "NetworkCore.h"
#include "NetworkConnection.h"
#include "NetworkServer.h"
#include "NetworkClient.h"
#include <memory>
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

// Simple logging interface
enum class LogLevel
{
	kDebug,
	kInfo,
	kWarning,
	kError
};

class ILogger
{
public:
	virtual ~ILogger() = default;
	virtual void Log(LogLevel level, const std::string& message) = 0;
};

// Default console logger implementation
class ConsoleLogger : public ILogger
{
public:
	void Log(LogLevel level, const std::string& message) override;
	void SetMinLevel(LogLevel min_level);

private:
	LogLevel min_level_ = LogLevel::kInfo;
	std::mutex log_mutex_;
};

// Global logger management
class Logger
{
public:
	static void SetLogger(std::shared_ptr<ILogger> logger);
	static void Log(LogLevel level, const std::string& message);
	static void Debug(const std::string& message);
	static void Info(const std::string& message);
	static void Warning(const std::string& message);
	static void Error(const std::string& message);

private:
	static std::shared_ptr<ILogger> logger_;
	static std::mutex logger_mutex_;
};

//==============================================================================
// Factory Classes
//==============================================================================

// Network component factory
class NetworkFactory
{
public:
	static std::unique_ptr<INetworkServer> CreateServer(
		std::unique_ptr<IConnectionManager> connection_manager = nullptr);

	static std::unique_ptr<INetworkClient> CreateClient();

	static std::unique_ptr<IConnectionManager> CreateConnectionManager();
};

// Configuration structures
struct ServerConfig
{
	Port port = 8080;
	int worker_thread_count = 0;  // 0 = auto-detect
	size_t max_connections = 1000;
	size_t receive_buffer_size = 8192;
	bool enable_logging = true;
	LogLevel log_level = LogLevel::kInfo;
};

struct ClientConfig
{
	int connection_timeout_ms = 5000;
	int reconnect_attempts = 3;
	int reconnect_delay_ms = 1000;
	size_t receive_buffer_size = 8192;
	bool auto_reconnect = false;
	bool enable_logging = true;
	LogLevel log_level = LogLevel::kInfo;
};