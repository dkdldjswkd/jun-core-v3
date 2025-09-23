#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer(std::shared_ptr<IOCPManager> manager) : Server(manager)
{
}

EchoServer::~EchoServer() 
{
}

void EchoServer::RegisterPacketHandlers()
{
	RegisterPacketHandler<echo::EchoRequest>([this](Session& _session, const echo::EchoRequest& request)
	{
		this->HandleEchoRequest(_session, request);
	});
}

void EchoServer::OnSessionDisconnect(Session* session)
{
	currentSessions_--;
	totalDisconnected_++;
	LOG_INFO("Client disconnected: Session 0x%llX", (uintptr_t)session);
}

void EchoServer::HandleEchoRequest(Session& _session, const echo::EchoRequest& request)
{
	//LOG_DEBUG("HandleEchoRequest : %s", request.message().c_str());
	
	echo::EchoResponse response;
	response.set_message(request.message());
	_session.SendPacket(response);

	return;
}

void EchoServer::OnSessionConnect(Session* session) 
{
	currentSessions_++;
	totalConnected_++;
	LOG_INFO("[0x%llX][JOIN] Client connected", (uintptr_t)session);
}

void EchoServer::OnServerStart()
{
	LOG_INFO("EchoServer started successfully!");
}

void EchoServer::OnServerStop() 
{
	LOG_INFO("EchoServer stopped successfully!");
}