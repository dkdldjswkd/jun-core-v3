#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer() : Server()
{
}

EchoServer::~EchoServer() 
{
}

void EchoServer::OnRecv(Session* session, PacketBuffer* cs_contentsPacket) 
{
	try {
		// 받은 메시지 처리
		auto cs_contentsPacket_len = cs_contentsPacket->GetPayloadSize();
		
		// 안전성 검사
		if (cs_contentsPacket_len <= 0 || cs_contentsPacket_len > 8000) {
			LOG_ERROR("Invalid packet size: %d", cs_contentsPacket_len);
			cs_contentsPacket->MoveRp(cs_contentsPacket_len);
			PacketBuffer::Free(cs_contentsPacket);
			return;
		}
		
		char* payloadData = new char[cs_contentsPacket_len + 1];
		cs_contentsPacket->GetData(payloadData, cs_contentsPacket_len);
		payloadData[cs_contentsPacket_len] = '\0';
		
		printf("[%04X][RECV:%d] %s\n", (session->session_id_.SESSION_UNIQUE & 0xFFFF), cs_contentsPacket_len, payloadData);

		// Echo back - 응답 패킷 생성
		PacketBuffer* sc_contentsPacket = PacketBuffer::Alloc();
		sc_contentsPacket->PutData(payloadData, cs_contentsPacket_len);
		delete[] payloadData;
		
		// 받은 패킷 포인터 이동 및 해제
		cs_contentsPacket->MoveRp(cs_contentsPacket_len);
		PacketBuffer::Free(cs_contentsPacket);

		// SendPacket은 패킷을 큐에 넣으므로 바로 Free하면 안됨
		SendPacket(session, sc_contentsPacket);
	}
	catch (const std::exception& e) {
		LOG_ERROR("Exception in OnRecv: %s", e.what());
		PacketBuffer::Free(cs_contentsPacket);
	}
	catch (...) {
		LOG_DEBUG("Unknown exception in OnRecv");
		PacketBuffer::Free(cs_contentsPacket);
	}
}

bool EchoServer::OnConnectionRequest(in_addr clientIP, WORD clientPort) {
	// 모든 클라이언트 허용
	return true;
}

void EchoServer::OnClientJoin(Session* session) 
{
	LOG_INFO("[%04X][JOIN] Client connected\n", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnClientLeave(Session* session) 
{
	printf("[%04X][LEAVE] Client disconnected\n", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnServerStart()
{
	LOG_INFO("EchoServer started successfully!");
}

void EchoServer::OnServerStop() 
{
	LOG_INFO("EchoServer stopped successfully!");
}

void EchoServer::OnError(int errorcode) 
{
}

