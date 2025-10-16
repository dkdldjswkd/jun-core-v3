#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
#include "EchoClient.h"
#include "echo_message.pb.h"
#include <iostream>
#include <string>
using namespace std;

static CrashDump dump;

int main() 
{
	try
	{
		// Logger 초기화 (비동기 모드)
		LOGGER_INITIALIZE_ASYNC(LOG_LEVEL_INFO);

		auto iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		LOG_ERROR_RETURN(iocpManager->IsValid(), -1, "Failed to create IOCPManager for client");

		EchoClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)), "127.0.0.1", 7777, 1);

		client.Initialize();
		client.StartClient();

		Sleep(2000);
		LOG_INFO("Starting chat. Type 'exit' to quit.");

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

			User* user = client.GetUser();
			if (!user) {
				cout << "[CLIENT] Not connected to server" << endl;
				continue;
			}

			// Protobuf 메시지로 서버에 전송
			echo::EchoRequest request;
			request.set_message(input);
			cout << "[CLIENT] Sending EchoRequest: " << input << endl;
			if (!user->SendPacket(request))
			{
				cout << "[CLIENT] Failed to send packet" << endl;
			}
		}

		LOG_INFO("chat ended");
		client.StopClient();
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