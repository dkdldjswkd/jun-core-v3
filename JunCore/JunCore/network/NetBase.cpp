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

void NetBase::InitializeIOCP(HANDLE sharedIOCP)
{
	// WSAStartup은 이미 생성자에서 호출됨
	
	// 공유 IOCP 사용
	if (sharedIOCP == INVALID_HANDLE_VALUE)
	{
		throw std::exception("Invalid shared IOCP handle");
	}
	
	h_iocp = sharedIOCP;
	// 공유 IOCP이므로 CloseHandle 하지 않도록 플래그 설정
	// (실제로는 소유권 관리가 필요하지만 간단히 구현)
}

void NetBase::CleanupIOCP()
{
	// 공유 IOCP인지 확인하는 로직이 필요하지만
	// 현재는 단순하게 구현 (실제로는 소유권 관리 필요)
	if (INVALID_HANDLE_VALUE != h_iocp)
	{
		CloseHandle(h_iocp);
		h_iocp = INVALID_HANDLE_VALUE;
	}

	WSACleanup();
}

//------------------------------
// 공유 IOCP 워커 스레드 (CompletionKey 기반 라우팅)
//------------------------------
void NetBase::RunSharedWorkerThread(HANDLE sharedIOCP)
{
	for (;;) 
	{
		DWORD ioSize = 0;
		NetBase* engine = nullptr;
		LPOVERLAPPED p_overlapped = nullptr;

		BOOL retGQCS = GetQueuedCompletionStatus(sharedIOCP, &ioSize, (PULONG_PTR)&engine, &p_overlapped, INFINITE);

		// 워커 스레드 종료
		if (ioSize == 0 && engine == nullptr && p_overlapped == nullptr) 
		{
			PostQueuedCompletionStatus(sharedIOCP, 0, 0, 0);
			return;
		}

		// 엔진이 nullptr인 경우는 에러
		if (engine == nullptr) 
		{
			continue;
		}

		// FIN 패킷
		if (ioSize == 0) 
		{
			// Session 포인터 복원 (overlapped 구조체의 위치로부터)
			Session* session = nullptr;
			if (p_overlapped != nullptr) {
				// overlapped가 recvOverlapped인지 sendOverlapped인지 확인
				// 이는 Session 구조체 내에서의 상대적 위치로 판단
				char* basePtr = (char*)p_overlapped;
				// recvOverlapped의 경우
				session = (Session*)(basePtr - offsetof(Session, recvOverlapped));
				// sendOverlapped와 구분하기 위해 validation 필요
				if (session->recvOverlapped.Internal != p_overlapped->Internal) {
					session = (Session*)(basePtr - offsetof(Session, sendOverlapped));
				}
			}
			
			if (session) {
				engine->HandleSessionDisconnect(session);
			}
			continue;
		}

		// PQCS (PostQueuedCompletionStatus)
		if ((ULONG_PTR)p_overlapped < 3) 
		{
			Session* session = (Session*)((char*)engine + (ULONG_PTR)p_overlapped); // 임시
			int pqcsType = (int)(ULONG_PTR)p_overlapped;
			switch (pqcsType) 
			{
				case 1: // SEND_POST
				{
					// 세션 정보는 별도 방법으로 전달 필요
					break;
				}
				case 2: // RELEASE_SESSION  
				{
					// 세션 정보는 별도 방법으로 전달 필요
					break;
				}
			}
			continue;
		}

		// Session 포인터 복원
		Session* session = nullptr;
		char* basePtr = (char*)p_overlapped;
		
		// overlapped 타입 판별 및 세션 포인터 복원
		Session* recvSession = (Session*)(basePtr - offsetof(Session, recvOverlapped));
		Session* sendSession = (Session*)(basePtr - offsetof(Session, sendOverlapped));
		
		// recv 완료처리
		if (p_overlapped == &recvSession->recvOverlapped) 
		{
			session = recvSession;
			if (retGQCS) 
			{
				session->recvBuf.MoveRear(ioSize);
				session->lastRecvTime = static_cast<DWORD>(GetTickCount64());
				engine->HandleRecvComplete(session);
			}
			engine->HandleDecrementIOCount(session);
		}
		// send 완료처리  
		else if (p_overlapped == &sendSession->sendOverlapped)
		{
			session = sendSession;
			if (retGQCS) 
			{
				engine->HandleSendComplete(session);
			}
			engine->HandleDecrementIOCount(session);
		}
	}
}