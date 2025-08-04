#include <iostream>
#include "EchoClient.h"
#include "../JunCommon/system/CrashDump.h"
using namespace std;

int main() 
{
	static CrashDump dump;

	EchoClient client("../ClientConfig.ini", "EchoClient");
	client.Start();
	printf("StartEchoClient \n");

	while (true) 
	{
		std::string input;
		cout << ">> ";
		std::cin >> input;

		if(input == "exit") 
		{
			break; 
		}

		// 서버에 테스트 메시지 전송
		PacketBuffer* testPacket = PacketBuffer::Alloc();
		testPacket->PutData(input.c_str(), input.length());
		client.SendPacket(testPacket);
		PacketBuffer::Free(testPacket);
	}

	client.Stop();
	Sleep(INFINITE);
}