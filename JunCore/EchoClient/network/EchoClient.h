#ifndef _ECHO_CLIENT_H_
#define _ECHO_CLIENT_H_

#include "../../NetCore/src/network/ClientManager.h"
#include <mutex>

class EchoClient
{
public:
	static EchoClient& Instance();

public:
	bool StartClient(int workerThreads);
	void StopClient();
	bool Connect(const std::string& ip, uint16 port);
	void Disconnect();
	
	void OnConnect(SessionPtr session_ptr);
	void OnDisconnect(SessionPtr session_ptr);
	void InitPacketHandlers();
	
public:
	SessionPtr session_ptr_ = nullptr;
	
private:
	ClientManager client_manager_;
	std::mutex session_mutex_;
};

#define sEchoClient EchoClient::Instance()
#endif