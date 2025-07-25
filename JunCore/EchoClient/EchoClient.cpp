﻿#include "EchoClient.h"
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
	auto csContentsPacket_len = csContentsPacket->GetPayloadSize();
	char* payloadData = new char[csContentsPacket_len + 1];
	payloadData[csContentsPacket_len] = 0;

	csContentsPacket->GetData(OUT payloadData, csContentsPacket_len);
	std::cout << "recv msg >> " << payloadData << std::endl;
	delete[] payloadData;
	csContentsPacket->MoveRp(csContentsPacket_len);
	return;
}

void EchoClient::OnDisconnect() 
{
	std::cout << "[EchoClient] Disconnected from server" << std::endl;
}

void EchoClient::OnClientStop() 
{
	std::cout << "[EchoClient] Client stopped" << std::endl;
}