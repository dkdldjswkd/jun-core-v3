#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
#include "../JunCore/protocol/handshake.pb.h"
#include "../JunCore/network/NetworkArchitecture.h"
using namespace std;

static CrashDump dump;

int main() 
{
	try
	{
		NetworkArchitecture::GetInstance().Initialize(3); // INIT_NETWORK_ARCH(2);
		std::shared_ptr<EchoServer> echoServer = NetworkArchitecture::GetInstance().CreateHandler<EchoServer>(); // CREATE_HANDLER(EchoServer);

		echoServer->Start();
		printf("\nEchoServer started\n");
		printf("--------------------------------------------------------\n");

		for (;;)
		{
			Sleep(1000);
		}

		echoServer->Stop();
		NetworkArchitecture::GetInstance().Shutdown(); // SHUTDOWN_NETWORK_ARCH();
	}
	catch (const std::exception& e)
	{
		printf("Exception caught in StartEchoServer: %s\n", e.what());
	}
	catch (...)
	{
		printf("Unknown exception caught in StartEchoServer\n");
	}
}