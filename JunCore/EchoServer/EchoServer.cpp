#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer() : Server()
{
	RegisterDirectPacketHandler<echo::EchoRequest>([this](const echo::EchoRequest& request) {
		this->HandleEchoRequest(request);
		});
}

EchoServer::~EchoServer() 
{
}

// PacketTest.cpp의 세션 저장 방식 - 현재 처리 중인 세션을 멤버로 관리
void EchoServer::HandleEchoRequest(const echo::EchoRequest& request)
{
	std::cout << "[SERVER] HandleEchoRequest: " << request.message() << std::endl;
	
	// EchoResponse 생성
	echo::EchoResponse response;
	response.set_message(request.message());
	response.set_timestamp(GetTickCount64());
	
	// 응답 패킷 직렬화
	std::vector<char> response_data = SerializeUnifiedPacket(response);
	
	// 현재 처리 중인 세션에 직접 raw 데이터 응답 전송 (PacketBuffer 없이)
	if (currentHandlingSession_) {
		bool success = SendRawData(currentHandlingSession_, response_data);
		std::cout << "[SERVER] Sent EchoResponse via raw send: " << response.message() 
				  << " (success=" << success << ")" << std::endl;
	}
	else {
		std::cout << "[SERVER] No current session to send response!" << std::endl;
	}
}

bool EchoServer::OnConnectionRequest(in_addr clientIP, WORD clientPort) {
	// 모든 클라이언트 허용
	return true;
}

void EchoServer::OnClientJoin(Session* session) 
{
	LOG_INFO("[%04X][JOIN] Client connected", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
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