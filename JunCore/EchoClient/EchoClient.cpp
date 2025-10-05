#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(std::shared_ptr<IOCPManager> manager) : Client(manager, "127.0.0.1", 7777)
{
}

EchoClient::~EchoClient()
{
}

void EchoClient::RegisterPacketHandlers()
{
	RegisterPacketHandler<echo::EchoResponse>([this](User& user, const echo::EchoResponse& response)
	{
		LOG_DEBUG("Received EchoResponse : %s", response.message().c_str());
	});
}

void EchoClient::OnSessionDisconnect(User* user)
{
	LOG_INFO("Disconnected from server. User: 0x%llX", (uintptr_t)user);
}