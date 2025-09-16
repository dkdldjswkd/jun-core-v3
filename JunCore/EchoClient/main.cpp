#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
#include "EchoClient.h"
#include <iostream>
#include <string>
using namespace std;

static CrashDump dump;

int main() 
{
	try
	{
		auto iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		LOG_ERROR_RETURN(iocpManager->IsValid(), -1, "Failed to create IOCPManager for client");

		EchoClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)));

		client.Initialize();
		LOG_ERROR_RETURN(client.Connect("127.0.0.1", 7777), -1, "Failed to connect to server");

		Sleep(1000);
		LOG_INFO("Starting chat. Type “exit” to quit.");

		while (true)
		{
			string input;
			cout << ">> ";

			if (!getline(cin, input)) {
				LOG_INFO("Input error or EOF. Exiting...");
				break;
			}

			if (input.empty()) {
				continue;
			}

			if (input == "exit") {
				LOG_INFO("Ending chat.");
				break;
			}

			// Protobuf 메시지로 서버에 전송
			client.SendEchoRequest(input);
		}

		LOG_INFO("Shutting down client...");
		client.Disconnect();
		LOG_INFO("Client has been shut down.");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Exception caught in client: %s", e.what());
		return -1;
	}
	catch (...)
	{
		LOG_ERROR("Unknown exception caught in client");
		return -1;
	}

	return 0;
}