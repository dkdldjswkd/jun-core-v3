#pragma once

#include "NetworkCore.h"
#include "NetworkServer.h"
#include "NetworkClient.h"
#include "NetworkConnection.h"
#include "Logger.h"
#include <memory>

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