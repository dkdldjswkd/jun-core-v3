#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <memory>
#include <atomic>
#include <string>
#include <functional>
#include <mutex>
#include <vector>

// Link required libraries
#pragma comment(lib, "ws2_32.lib")



	//==============================================================================
	// Common Network Interfaces and Types
	//==============================================================================

using ConnectionId = int;
using Port = unsigned short;
using ByteBuffer = std::vector<char>;

// Connection state enumeration
enum class ConnectionState
{
	kDisconnected,
	kConnecting,
	kConnected,
	kDisconnecting
};

// Network error codes
enum class NetworkError
{
	kSuccess,
	kInitializationFailed,
	kConnectionFailed,
	kSendFailed,
	kReceiveFailed,
	kInvalidParameter,
	kTimeout,
	kUnknownError
};

// Forward declarations
class IConnection;
class INetworkEventHandler;

//==============================================================================
// Core Network Interfaces
//==============================================================================

// Base connection interface
class IConnection
{
public:
	virtual ~IConnection() = default;

	// Basic connection info
	virtual ConnectionId GetId() const = 0;
	virtual SOCKET GetSocket() const = 0;
	virtual ConnectionState GetState() const = 0;
	virtual bool IsValid() const = 0;

	// Data transmission
	virtual NetworkError Send(const char* data, size_t length) = 0;
	virtual NetworkError Send(const std::string& message) = 0;
	virtual NetworkError Send(const ByteBuffer& buffer) = 0;

	// Connection management
	virtual void Disconnect() = 0;
	virtual std::string GetRemoteAddress() const = 0;
	virtual Port GetRemotePort() const = 0;
};

// Network event handler interface
class INetworkEventHandler
{
public:
	virtual ~INetworkEventHandler() = default;

	// Connection events
	virtual void OnConnected(std::shared_ptr<IConnection> connection) = 0;
	virtual void OnDisconnected(std::shared_ptr<IConnection> connection) = 0;

	// Data events
	virtual void OnDataReceived(std::shared_ptr<IConnection> connection,
		const char* data, size_t length) = 0;

	// Error events
	virtual void OnError(std::shared_ptr<IConnection> connection,
		NetworkError error, const std::string& message) = 0;
};

// Server-specific event handler interface
class IServerEventHandler : public INetworkEventHandler
{
public:
	virtual void OnClientAccepted(std::shared_ptr<IConnection> client) = 0;
};

// Client-specific event handler interface  
class IClientEventHandler : public INetworkEventHandler
{
public:
	virtual void OnConnectionAttempt(const std::string& address, Port port) = 0;
	virtual void OnConnectionEstablished() = 0;
	virtual void OnConnectionLost() = 0;
};