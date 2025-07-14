#pragma once
#include "../NetCore/src/network/ServerManager.h"
#include <string>
#include <memory>

class GameServerManager
{
public:
	GameServerManager();
	~GameServerManager();

	bool Start(uint16 port, int worker_count);
	void Stop();

	void RunConsoleCommands();

private:
	void OnClientAccepted(SessionPtr session);
	void OnClientDisconnected(SessionPtr session);
	void OnPacketReceived(SessionPtr session, int32 packet_id, const std::vector<char>& serialized_packet);

	ServerManager server_manager_;
};