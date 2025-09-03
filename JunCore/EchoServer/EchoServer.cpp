#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer() : NetBase()
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
			printf("[ERROR] Invalid packet size: %d\n", cs_contentsPacket_len);
			cs_contentsPacket->MoveRp(cs_contentsPacket_len);
			PacketBuffer::Free(cs_contentsPacket);
			return;
		}
		
		char* payloadData = new char[cs_contentsPacket_len + 1];
		cs_contentsPacket->GetData(payloadData, cs_contentsPacket_len);
		payloadData[cs_contentsPacket_len] = '\0';
		
		printf("[%04X][RECV:%d] %s\n", (session->sessionId.SESSION_UNIQUE & 0xFFFF), cs_contentsPacket_len, payloadData);

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

bool EchoServer::OnConnectionRequest(in_addr ip, WORD port) {
	return true;
}

void EchoServer::OnClientJoin(Session* session) {
	printf("[%04X][JOIN] Client connected\n", (session->sessionId.SESSION_UNIQUE & 0xFFFF));
	
	PacketBuffer* sc_packet = PacketBuffer::Alloc();
	*sc_packet << 0x7fffffffffffffff;
	SendPacket(session, sc_packet);
	// SendPacket이 패킷을 큐에 넣으므로 Free하지 않음
}

void EchoServer::OnClientLeave(Session* session) 
{
	printf("[%04X][LEAVE] Client disconnected\n", (session->sessionId.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnServerStop() 
{
}

void EchoServer::OnError(int errorcode) 
{
}

//------------------------------
// 새로운 아키텍처의 Start/Stop 구현
//------------------------------
void EchoServer::Start() 
{
    if (!IsIOCPManagerAttached()) 
	{
		LOG_ERROR("IOCP is not Attached!!");
        return;
    }
    
    socketManager = std::make_unique<ServerSocketManager>(iocpManager, this);
    
    // 서버 시작 (기본 설정: 0.0.0.0:7777, 최대 1000 세션)
    if (socketManager->StartServer("0.0.0.0", 7777, 1000)) 
	{
		LOG_DEBUG("EchoServer started successfully on port 7777!");
    } 
	else 
	{
		LOG_ERROR("EchoServer failed to start!");
        socketManager.reset();
    }
}

void EchoServer::Stop() 
{
    printf("EchoServer stopping...\n");
    
    if (socketManager) {
        socketManager->StopServer();
        socketManager.reset();
    }
    
    OnServerStop();
    printf("EchoServer stopped successfully!\n");
}