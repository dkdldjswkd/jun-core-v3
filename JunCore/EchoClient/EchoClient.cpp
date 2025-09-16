#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(std::shared_ptr<IOCPManager> manager) : Client(manager)
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

void EchoClient::OnDisconnect()
{
	LOG_INFO("Disconnected from server.");
}

bool EchoClient::SendEchoRequest(const std::string& message)
{
	if (!IsConnected())
	{
		std::cout << "[CLIENT] Not connected to server!" << std::endl;
		return false;
	}

	// EchoRequest 메시지 생성
	echo::EchoRequest request;
	request.set_message(message);

	std::cout << "[CLIENT] Sending EchoRequest: " << message << std::endl;

	// Session에서 직접 protobuf 메시지 전송
	bool success = GetCurrentSession()->SendPacket(request);
	if (!success) {
		std::cout << "[CLIENT] Failed to send packet" << std::endl;
	}

	return success;
}