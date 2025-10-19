#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient(std::shared_ptr<IOCPManager> manager, const char* serverIP, WORD port, int targetConnectionCount)
	: Client(manager, serverIP, port, targetConnectionCount)
{
}

EchoClient::~EchoClient()
{
	StopClient();
}

void EchoClient::RegisterPacketHandlers()
{
	RegisterPacketHandler<echo::EchoResponse>([this](User& user, const echo::EchoResponse& response)
	{
		LOG_DEBUG("Received EchoResponse : %s", response.message().c_str());
	});
}


void EchoClient::OnClientStart()
{
	LOG_INFO("EchoClient starting - console input thread enabled");
	inputRunning_.store(true, std::memory_order_release);
	consoleInputThread_ = std::thread(&EchoClient::ConsoleInputThreadFunc, this);
}

void EchoClient::OnClientStop()
{
	LOG_INFO("EchoClient stopping - waiting for console input thread");
	inputRunning_.store(false, std::memory_order_release);

	if (consoleInputThread_.joinable())
	{
		consoleInputThread_.join();
	}
}

void EchoClient::OnConnectComplete(User* user, bool success)
{
	if (success)
	{
		connectedUser = user;
		LOG_INFO("Successfully connected to server! User: 0x%llX", (uintptr_t)user);

		// 연결 성공 시 테스트 메시지 전송
		echo::EchoRequest request;
		request.set_message("Hello from async client!");
		user->SendPacket(request);
	}
	else
	{
		LOG_ERROR("Failed to connect to server");
	}
}

void EchoClient::ConsoleInputThreadFunc()
{
	std::cout << "Console input thread started. Type messages to send to server (or 'quit' to exit):" << std::endl;

	while (inputRunning_.load(std::memory_order_acquire))
	{
		std::string input;
		std::getline(std::cin, input);

		if (input == "quit")
		{
			LOG_INFO("User requested quit");
			break;
		}

		// 연결된 유저가 있으면 메시지 전송
		if (connectedUser)
		{
			echo::EchoRequest request;
			request.set_message(input);
			connectedUser->SendPacket(request);
			LOG_DEBUG("Sent message: %s", input.c_str());
		}
		else
		{
			LOG_WARN("Not connected to server yet");
		}
	}

	LOG_INFO("Console input thread stopped");
}