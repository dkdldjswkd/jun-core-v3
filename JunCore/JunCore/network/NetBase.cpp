#include <WS2tcpip.h>
#include <WinSock2.h>
#include "NetBase.h"
#pragma comment(lib, "ws2_32.lib")

//------------------------------
// NetBase 구현
//------------------------------

void NetBase::InitializeProtocol(const char* systemFile, const char* configSection)
{
	// TODO: Parser를 사용한 설정 로딩 구현 예정
	// 현재는 하드코딩된 값으로 초기화 (EchoServer/Client는 LAN 모드 사용)
	protocolCode = 0xFF;
	privateKey = 0xFF; 
	netType = NetType::LAN;

	/* 향후 구현 예정:
	Parser parser;
	parser.LoadFile(systemFile);
	parser.GetValue(configSection, "PROTOCOL_CODE", (int*)&protocolCode);
	parser.GetValue(configSection, "PRIVATE_KEY", (int*)&privateKey);
	parser.GetValue(configSection, "NET_TYPE", (int*)&netType);
	*/
}

void NetBase::InitializeIOCP(DWORD maxConcurrentThreads)
{
	// WSAStartup은 이미 생성자에서 호출됨
	
	// IOCP 생성 
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, maxConcurrentThreads);
	if (INVALID_HANDLE_VALUE == h_iocp) 
	{
		// LOG("NetBase", LOG_LEVEL_FATAL, "CreateIoCompletionPort() Error: %d", GetLastError());
		throw std::exception("CreateIoCompletionPort_ERROR");
	}
}

void NetBase::CleanupIOCP()
{
	if (INVALID_HANDLE_VALUE != h_iocp)
	{
		CloseHandle(h_iocp);
		h_iocp = INVALID_HANDLE_VALUE;
	}

	WSACleanup();
}