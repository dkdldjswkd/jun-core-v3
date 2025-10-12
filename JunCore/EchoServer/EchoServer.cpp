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
	RegisterPacketHandler<echo::EchoRequest>([this](User& user, const echo::EchoRequest& request)
	{
		this->HandleEchoRequest(user, request);
	});
}

void EchoServer::OnUserDisconnect(User* user)
{
	currentSessions_--;
	totalDisconnected_++;
	LOG_INFO("Client disconnected: User 0x%llX", (uintptr_t)user);
}

void EchoServer::HandleEchoRequest(User& user, const echo::EchoRequest& request)
{
	//LOG_DEBUG("HandleEchoRequest : %s", request.message().c_str());
	
	echo::EchoResponse response;
	response.set_message(request.message());
	user.SendPacket(response);

	return;
}

void EchoServer::OnSessionConnect(User* user) 
{
	currentSessions_++;
	totalConnected_++;
	LOG_INFO("[0x%llX][JOIN] Client connected", (uintptr_t)user);
}

void EchoServer::OnServerStart()
{
	LOG_INFO("EchoServer started successfully!");
}

void EchoServer::OnServerStop() 
{
	LOG_INFO("EchoServer stopped successfully!");
}

