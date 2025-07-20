#include <iostream>
#include "EchoServer.h"
#include "../JunCommon/lib/CrashDump.h"
using namespace std;

// EchoServer.cpp, Define Ȯ��
void StartEchoServer() 
{
	EchoServer server("../ServerConfig.ini", "EchoServer");
	server.Start();
	printf("StartEchoServer \n");

	for (;;) {
		// 1�� �ֱ� ����͸�
		Sleep(1000);
		printf("NetworkLib ---------------------------------------------------- \n");
		printf("sessionCount    : %d \n", server.GetSessionCount()); 
		printf("PacketCount     : %d \n", 0); // PacketBuffer::Get_UseCount() not available
		printf("acceptTotal     : %d \n", server.GetAcceptTotal());
		printf("acceptTPS       : %d \n", server.GetAcceptTPS());
		printf("sendMsgTPS      : %d \n", server.GetSendTPS());
		printf("recvMsgTPS      : %d \n", server.GetRecvTPS());
		printf("\n\n\n\n\n\n\n\n\n\n \n\n\n\n\n\n\n\n\n\n \n\n");
	}

	server.Stop();
}

int main() {
	static CrashDump dump;
	StartEchoServer();
	Sleep(INFINITE);
}