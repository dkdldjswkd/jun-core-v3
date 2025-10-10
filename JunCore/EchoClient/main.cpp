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

		EchoClient client(std::shared_ptr<IOCPManager>(std::move(iocpManager)));

		client.Initialize();
		User* user = client.Connect();
		LOG_ERROR_RETURN(user != nullptr, -1, "Failed to connect to server");

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
			echo::EchoRequest request;
			request.set_message(input);
			cout << "[CLIENT] Sending EchoRequest: " << input << endl;
			if (!user->SendPacket(request)) 
			{
				cout << "[CLIENT] Failed to send packet" << endl;
			}
		}

		LOG_INFO("chat ended");
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