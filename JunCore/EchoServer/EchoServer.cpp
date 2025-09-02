#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"
#include "../JunCore/network/NetworkArchitecture.h"

EchoServer::EchoServer(const char* systemFile, const char* server) : NetBase(systemFile, server)
{
}

EchoServer::~EchoServer() 
{
}

void EchoServer::OnRecv(Session* session, PacketBuffer* cs_contentsPacket) 
{
	printf("!!! [OnRecv CALLED] SessionID: %lld !!!\n", session->sessionId.sessionId);
	
	try {
		// 받은 메시지 출력
		auto cs_contentsPacket_len = cs_contentsPacket->GetPayloadSize();
		printf("!!! Packet payload size: %d !!!\n", cs_contentsPacket_len);
		
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
		
		printf("!!! [RECV] SessionID: %lld | Length: %d | Message: %s !!!\n", 
			session->sessionId.sessionId, cs_contentsPacket_len, payloadData);

		// Echo back - 응답 패킷 생성
		PacketBuffer* sc_contentsPacket = PacketBuffer::Alloc();
		sc_contentsPacket->PutData(payloadData, cs_contentsPacket_len);
		delete[] payloadData;
		
		// 받은 패킷 포인터 이동 및 해제
		cs_contentsPacket->MoveRp(cs_contentsPacket_len);
		PacketBuffer::Free(cs_contentsPacket);

		printf("[SEND] SessionID: %lld | Echo response sent\n", session->sessionId.sessionId);
		// SendPacket은 패킷을 큐에 넣으므로 바로 Free하면 안됨
		SendPacket(session, sc_contentsPacket);
	}
	catch (const std::exception& e) {
		printf("[ERROR] Exception in OnRecv: %s\n", e.what());
		PacketBuffer::Free(cs_contentsPacket);
	}
	catch (...) {
		printf("[ERROR] Unknown exception in OnRecv\n");
		PacketBuffer::Free(cs_contentsPacket);
	}
}

bool EchoServer::OnConnectionRequest(in_addr ip, WORD port) {
	printf("*** [CONNECTION_REQUEST] IP: %s | PORT: %u ***\n", inet_ntoa(ip), port);
	return true;
}

void EchoServer::OnClientJoin(Session* session) {
	acceptTotal++;  // 접속 카운터 증가
	printf("*** [CLIENT_JOIN] SessionID: %lld ***\n", session->sessionId.sessionId);
	
	PacketBuffer* sc_packet = PacketBuffer::Alloc();
	*sc_packet << 0x7fffffffffffffff;
	SendPacket(session, sc_packet);
	
	printf("*** [WELCOME] SessionID: %lld | Welcome packet sent ***\n", session->sessionId.sessionId);
	// SendPacket이 패킷을 큐에 넣으므로 Free하지 않음
}

void EchoServer::OnClientLeave(Session* session) 
{
	printf("[CLIENT_LEAVE] SessionID: %lld\n", session->sessionId.sessionId);
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
    printf("🚀 EchoServer starting with new architecture...\n");
    
    // ServerSocketManager 생성 및 시작
    if (!IsIOCPManagerAttached()) {
        printf("❌ IOCP Manager not attached!\n");
        return;
    }
    
    socketManager = std::make_unique<ServerSocketManager>(iocpManager, this);
    
    // 서버 시작 (기본 설정: 0.0.0.0:7777, 최대 1000 세션)
    if (socketManager->StartServer("0.0.0.0", 7777, 1000)) {
        printf("✅ EchoServer started successfully on port 7777!\n");
    } else {
        printf("❌ EchoServer failed to start!\n");
        socketManager.reset();
    }
}

void EchoServer::Stop() 
{
    printf("🛑 EchoServer stopping...\n");
    
    if (socketManager) {
        socketManager->StopServer();
        socketManager.reset();
    }
    
    OnServerStop();
    printf("✅ EchoServer stopped successfully!\n");
}