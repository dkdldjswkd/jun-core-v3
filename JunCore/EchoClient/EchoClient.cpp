#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient() : Client()
{
	RegisterDirectPacketHandler<echo::EchoResponse>([this](const echo::EchoResponse& response) {
		std::cout << "[CLIENT] Received EchoResponse: " << response.message() << " (timestamp: " << response.timestamp() << ")" << std::endl;
		});
}

EchoClient::~EchoClient()
{
}

void EchoClient::OnClientJoin(Session* session)
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

	std::vector<char> packet_data = SerializeUnifiedPacket(request);

	std::cout << "[CLIENT] Sending EchoRequest: " << message << std::endl;

	// 직접 raw 데이터 전송 (PacketBuffer 없이)
	bool success = SendRawData(GetCurrentSession(), packet_data);
	if (!success) {
		std::cout << "[CLIENT] Failed to send packet" << std::endl;
	}

	return success;
}