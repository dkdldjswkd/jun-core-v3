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
		testPacket->PutData(input.c_str(), static_cast<int>(input.length()));
		client.SendPacket(testPacket);
		// SendPacket은 패킷을 큐에 넣으므로 바로 Free하면 안됨
	}

	client.Stop();
	Sleep(INFINITE);
}