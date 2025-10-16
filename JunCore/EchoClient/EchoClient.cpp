#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(std::shared_ptr<IOCPManager> manager, const char* serverIP, WORD port, int targetConnectionCount)
	: Client(manager, serverIP, port, targetConnectionCount)
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


void EchoClient::OnConnectComplete(User* user, bool success)
{
	if (success)
	{
		connectedUser = user;
		LOG_INFO("Successfully connected to server! User: 0x%llX", (uintptr_t)user);

		// 연결 성공 시 테스트 메시지 전송
		echo::EchoRequest request;
		request.set_message("Hello from async client!");
		user->SendPacket(request);
	}
	else
	{
		LOG_ERROR("Failed to connect to server");
	}
}