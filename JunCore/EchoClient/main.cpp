#include <iostream>
#include "EchoClient.h"
#include "../JunCommon/lib/CrashDump.h"
using namespace std;

void StartEchoClient() 
{
	EchoClient client("../ClientConfig.ini", "EchoClient");
	client.Start();
	printf("StartEchoClient \n");

	for (;;) 
	{
		// 1초 주기 모니터링
		Sleep(1000);
		printf("EchoClient ---------------------------------------------------- \n");
		printf("sendMsgTPS      : %d \n", client.GetSendTPS());
		printf("recvMsgTPS      : %d \n", client.GetRecvTPS());
		printf("\n\n\n\n\n\n\n\n\n\n \n\n\n\n\n\n\n\n\n\n \n\n");
		
		client.UpdateTPS();
	}

	client.Stop();
}

int main() 
{
	static CrashDump dump;
	StartEchoClient();
	Sleep(INFINITE);
}