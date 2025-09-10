#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer() : Server()
{
	RegisterDirectPacketHandler<echo::EchoRequest>([this](Session& _session, const echo::EchoRequest& request) 
	{
		this->HandleEchoRequest(_session, request);
	});
}

EchoServer::~EchoServer() 
{
}

// PacketTest.cpp의 세션 저장 방식 - 현재 처리 중인 세션을 멤버로 관리
void EchoServer::HandleEchoRequest(Session& _session, const echo::EchoRequest& request)
{
	std::cout << "[SERVER] HandleEchoRequest: " << request.message() << std::endl;
	
	// EchoResponse 생성
	echo::EchoResponse response;
	response.set_message(request.message());
	
	SendPacket(_session, response);
}

bool EchoServer::OnConnectionRequest(in_addr clientIP, WORD clientPort) 
{
	// 모든 클라이언트 허용
	return true;
}

void EchoServer::OnClientJoin(Session* session) 
{
	LOG_INFO("[%04X][JOIN] Client connected", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnClientLeave(Session* session) 
{
	LOG_INFO("[%04X][LEAVE] Client disconnected", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnServerStart()
{
	LOG_INFO("EchoServer started successfully!");
}

void EchoServer::OnServerStop() 
{
	LOG_INFO("EchoServer stopped successfully!");
}