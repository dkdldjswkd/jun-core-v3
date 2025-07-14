#pragma once
#include "../NetCore/src/network/ClientManager.h"
#include <string>
#include <memory>

class TestClientManager
{
public:
	TestClientManager();
	~TestClientManager();

	bool Connect(const std::string& ip_address, uint16 port);
	void Disconnect();

	void RunAutomatedTest();
	void RunInteractiveSession();

private:
	void OnConnected(SessionPtr session);
	void OnDisconnected(SessionPtr session);
	void OnPacketReceived(SessionPtr session, int32 packet_id, const std::vector<char>& serialized_packet);

	ClientManager client_manager_;
};