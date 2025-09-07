#include <iostream>
#include <string>
#include "EchoClient.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/network/IOCPManager.h"
using namespace std;

static CrashDump dump;

int main()
{
	try
	{
		std::unique_ptr<IOCPManager> iocpManager = IOCPManager::Create().WithWorkerCount(2).Build();
		LOG_ERROR_RETURN(iocpManager->IsValid(), -1, "Failed to create IOCPManager for client");

		EchoClient client;
		client.AttachIOCPManager(std::shared_ptr<IOCPManager>(std::move(iocpManager)));
		LOG_ERROR_RETURN(client.Connect("127.0.0.1", 7777), -1, "Failed to connect to server");

		Sleep(1000);
		cout << "=== 채팅을 시작합니다. 'exit' 입력시 종료 ===" << endl;

		while (true)
		{
			string input;
			cout << ">> ";

			if (!getline(cin, input)) 
			{
				cout << "\n입력 오류 또는 EOF. 종료합니다..." << endl;
				break;
			}

			if (input.empty()) 
			{
				continue;
			}

			if (input == "exit") 
			{
				cout << "채팅을 종료합니다." << endl;
				break;
			}

			// Protobuf 메시지로 서버에 전송
			client.SendEchoRequest(input);
		}

		cout << "클라이언트를 종료합니다..." << endl;
		client.Disconnect();
		client.DetachIOCPManager();
		cout << "클라이언트가 종료되었습니다." << endl;
	}
	catch (const std::exception& e)
	{
		printf("Exception caught in client: %s\n", e.what());
		return -1;
	}
	catch (...)
	{
		printf("Unknown exception caught in client\n");
		return -1;
	}

	return 0;
}