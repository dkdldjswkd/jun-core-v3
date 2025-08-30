#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(const char* systemFile, const char* client) : NetworkEngine<ClientPolicy>(systemFile, client)
{
}

EchoClient::~EchoClient() 
{
}

void EchoClient::OnConnect() 
{
	std::cout << "[EchoClient] Connected to server" << std::endl;
	
	// 서버에 테스트 메시지 전송
	PacketBuffer* testPacket = PacketBuffer::Alloc();
	const char* message = "Hello Server!";
	testPacket->PutData(message, static_cast<int>(strlen(message)));
	
	std::cout << "[DEBUG] Sending initial packet: " << message << std::endl;
	SendPacket(testPacket);
	std::cout << "[DEBUG] SendPacket completed" << std::endl;
	// SendPacket이 패킷을 큐에 넣으므로 Free하지 않음
}

void EchoClient::OnRecv(PacketBuffer* csContentsPacket) 
{
	auto csContentsPacket_len = csContentsPacket->GetPayloadSize();
	char* payloadData = new char[csContentsPacket_len + 1];
	payloadData[csContentsPacket_len] = 0;

	csContentsPacket->GetData(payloadData, csContentsPacket_len);
	std::cout << "[EchoClient] recv msg >> " << payloadData << std::endl;
	delete[] payloadData;
	csContentsPacket->MoveRp(csContentsPacket_len);
	
	// 중요: PacketBuffer 해제
	PacketBuffer::Free(csContentsPacket);
}

void EchoClient::OnDisconnect() 
{
	std::cout << "[EchoClient] Disconnected from server" << std::endl;
}

void EchoClient::OnClientStop() 
{
	std::cout << "[EchoClient] Client stopped" << std::endl;
}