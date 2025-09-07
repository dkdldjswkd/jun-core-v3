#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient() : Client()
{
}

EchoClient::~EchoClient() 
{
}

void EchoClient::OnRecv(Session* session, PacketBuffer* packet)
{
    auto payloadSize = packet->GetPayloadSize();
    LOG_DEBUG("[RECV] Payload size: %d", payloadSize);
    
    if (payloadSize <= 0 || 8000 < payloadSize)
    {
        LOG_ERROR("Invalid payload size: %d", payloadSize);
        PacketBuffer::Free(packet);
        return;
    }
    
    // 8바이트 정수인지 확인 (서버의 OnClientJoin에서 보내는 패킷)
    if (payloadSize == 8) 
    {
        int64_t joinMessage = 0;
        *packet >> joinMessage;
        printf("[EchoClient][RECV] Received join message: 0x%llx\n", joinMessage);
    } 
    else 
    {
        // 문자열 메시지 처리
        char* payloadData = new char[payloadSize + 1];
        payloadData[payloadSize] = '\0';

        packet->GetData(payloadData, payloadSize);
        printf("[EchoClient][RECV] Received message: '%s'\n", payloadData);
        delete[] payloadData;
        packet->MoveRp(payloadSize);
    }
    
    PacketBuffer::Free(packet);
}

void EchoClient::OnClientJoin(Session* session) 
{
	LOG_INFO("Connected to server successfully! Session ID: %lld", session->session_id_.sessionId);
}

void EchoClient::OnDisconnect()
{
	LOG_INFO("Disconnected from server.");
}