#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(const char* systemFile, const char* client) : NetClient(systemFile, client)
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
	SendPacket(testPacket);
	PacketBuffer::Free(testPacket);
}

void EchoClient::OnRecv(PacketBuffer* csContentsPacket) 
{
	std::cout << "[EchoClient] Received echo from server, size: " << csContentsPacket->GetPayloadSize() << std::endl;
	
	// 받은 패킷을 다시 전송 (echo)
	PacketBuffer* echoPacket = PacketBuffer::Alloc();
	auto payloadSize = csContentsPacket->GetPayloadSize();
	char* payloadData = new char[payloadSize];
	csContentsPacket->GetData(payloadData, payloadSize);
	echoPacket->PutData(payloadData, payloadSize);
	delete[] payloadData;
	
	SendPacket(echoPacket);
	PacketBuffer::Free(echoPacket);
}

void EchoClient::OnDisconnect() 
{
	std::cout << "[EchoClient] Disconnected from server" << std::endl;
}

void EchoClient::OnClientStop() 
{
	std::cout << "[EchoClient] Client stopped" << std::endl;
}