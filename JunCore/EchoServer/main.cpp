#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/system/CrashDump.h"
using namespace std;

// EchoServer.cpp, Define 확인
void StartEchoServer()
{
	EchoServer server("../ServerConfig.ini", "EchoServer");
	server.Start();
	printf("StartEchoServer\n");

	for (;;) {
		// 1초 주기로 출력
		Sleep(1000);
		printf("NetworkLib ---------------------------------------------------- \n");
		printf("세션 수          : %d \n", server.GetSessionCount());
		printf("패킷 수          : %d \n", 0); // PacketBuffer::Get_UseCount() 사용 불가
		printf("총 접속 수       : %d \n", server.GetAcceptTotal());
		printf("초당 접속 수     : %d \n", server.GetAcceptTPS());
		printf("초당 전송 메시지 : %d \n", server.GetSendTPS());
		printf("초당 수신 메시지 : %d \n", server.GetRecvTPS());
		printf("\n\n\n\n\n\n\n\n\n\n \n\n\n\n\n\n\n\n\n\n \n\n");
	}

	server.Stop();
}

int main() {
	static CrashDump dump;
	StartEchoServer();
	Sleep(INFINITE);
}