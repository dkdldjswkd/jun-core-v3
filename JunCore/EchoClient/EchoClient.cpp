#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient() : Client()
{
	RegisterDirectPacketHandler<echo::EchoResponse>([this](Session& session, const echo::EchoResponse& response) {
		std::cout << "[CLIENT] Received EchoResponse: " << response.message() << " (timestamp: " << response.timestamp() << ")" << std::endl;
	});
}

EchoClient::~EchoClient()
{
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

	// SendPacket으로 직접 protobuf 메시지 전송
	bool success = NetBase::SendPacket(*GetCurrentSession(), request);
	if (!success) {
		std::cout << "[CLIENT] Failed to send packet" << std::endl;
	}

	return success;
}