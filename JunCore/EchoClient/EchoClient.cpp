#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient() : Client()
{
	// Protobuf 패킷 핸들러 등록 (PacketTest.cpp 방식)
	RegisterDirectPacketHandler<echo::EchoResponse>(
		[this](const echo::EchoResponse& response) {
			std::cout << "[CLIENT] Received EchoResponse: " << response.message() 
					  << " (timestamp: " << response.timestamp() << ")" << std::endl;
		}
	);
}

EchoClient::~EchoClient() 
{
}

void EchoClient::OnRecv(Session* session, PacketBuffer* packet)
{
	try {
		// 패킷 길이 검사
		auto packet_len = packet->GetPayloadSize();
		if (packet_len <= 0 || packet_len > 8000) {
			LOG_ERROR("Invalid packet size: %d", packet_len);
			PacketBuffer::Free(packet);
			return;
		}

		// raw 데이터 추출
		std::vector<char> packet_data(packet_len);
		packet->GetData(packet_data.data(), packet_len);
		packet->MoveRp(packet_len);
		PacketBuffer::Free(packet);

		// 패킷 처리 (PacketTest.cpp와 동일)
		bool processed = ProcessDirectPacket(packet_data.data(), packet_len);
		if (!processed) {
			std::cout << "[CLIENT] Failed to process direct packet" << std::endl;
		}
	}
	catch (const std::exception& e) {
		LOG_ERROR("Exception in OnRecv: %s", e.what());
		PacketBuffer::Free(packet);
	}
	catch (...) {
		LOG_ERROR("Unknown exception in OnRecv");
		PacketBuffer::Free(packet);
	}
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

	std::vector<char> packet_data = SerializeDirectPacket(request);

	std::cout << "[CLIENT] Sending EchoRequest: " << message << std::endl;

	// 직접 raw 데이터 전송 (PacketBuffer 없이)
	bool success = SendRawData(GetCurrentSession(), packet_data);
	if (!success) {
		std::cout << "[CLIENT] Failed to send packet" << std::endl;
	}

	return success;
}