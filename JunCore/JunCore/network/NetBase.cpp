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

void NetBase::InitializeIOCP(HANDLE existingIOCP)
{
	// WSAStartup은 이미 생성자에서 호출됨
	
	// 기존 IOCP 사용
	if (existingIOCP == INVALID_HANDLE_VALUE)
	{
		throw std::exception("Invalid IOCP handle");
	}
	
	h_iocp = existingIOCP;
	// 기존 IOCP이므로 CloseHandle 하지 않도록 플래그 설정
	// (실제로는 소유권 관리가 필요하지만 간단히 구현)
}

void NetBase::CleanupIOCP()
{
	// IOCP 소유권 확인이 필요하지만
	// 현재는 단순하게 구현 (실제로는 소유권 관리 필요)
	if (INVALID_HANDLE_VALUE != h_iocp)
	{
		CloseHandle(h_iocp);
		h_iocp = INVALID_HANDLE_VALUE;
	}

	WSACleanup();
}

//------------------------------
// IOCP 워커 스레드 (CompletionKey 기반 라우팅)
//------------------------------
void NetBase::RunWorkerThread(HANDLE iocpHandle)
{
	for (;;) 
	{
		DWORD ioSize = 0;
		Session* session = nullptr;
		LPOVERLAPPED p_overlapped = nullptr;

		BOOL retGQCS = GetQueuedCompletionStatus(iocpHandle, &ioSize, (PULONG_PTR)&session, &p_overlapped, INFINITE);

		// 워커 스레드 종료
		if (ioSize == 0 && session == nullptr && p_overlapped == nullptr) 
		{
			PostQueuedCompletionStatus(iocpHandle, 0, 0, 0);
			return;
		}

		// 세션이 nullptr인 경우는 에러
		if (session == nullptr) 
		{
			continue;
		}
		
		// 세션에서 엔진 가져오기
		NetBase* engine = session->GetEngine();
		if (engine == nullptr)
		{
			continue;
		}

		// FIN 패킷
		if (ioSize == 0) 
		{
			engine->HandleSessionDisconnect(session);
			continue;
		}

		// PQCS (PostQueuedCompletionStatus)
		if ((ULONG_PTR)p_overlapped < 3) 
		{
			int pqcsType = (int)(ULONG_PTR)p_overlapped;
			switch (pqcsType) 
			{
				case 1: // SEND_POST
				{
					// TODO: 필요시 구현
					break;
				}
				case 2: // RELEASE_SESSION  
				{
					// TODO: 필요시 구현
					break;
				}
			}
			continue;
		}

		// recv 완료처리
		if (p_overlapped == &session->recvOverlapped) 
		{
			if (retGQCS) 
			{
				session->recvBuf.MoveRear(ioSize);
				session->lastRecvTime = static_cast<DWORD>(GetTickCount64());
				engine->HandleRecvComplete(session);
			}
			engine->HandleDecrementIOCount(session);
		}
		// send 완료처리  
		else if (p_overlapped == &session->sendOverlapped)
		{
			if (retGQCS) 
			{
				engine->HandleSendComplete(session);
			}
			engine->HandleDecrementIOCount(session);
		}
	}
}