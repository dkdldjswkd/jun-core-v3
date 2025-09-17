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
	RegisterPacketHandler<echo::EchoResponse>([this](Session& session, const echo::EchoResponse& response)
		{
			LOG_DEBUG("Received EchoResponse : %s", response.message().c_str());
		});
}

void EchoClient::OnConnect(Session* session)
{
	LOG_INFO("Connected to server successfully! Session ID: %lld", session->session_id_.sessionId);
}

void EchoClient::OnDisconnect(Session* session)
{
	LOG_INFO("Disconnected from server. Session ID: %lld", session->session_id_.sessionId);
}