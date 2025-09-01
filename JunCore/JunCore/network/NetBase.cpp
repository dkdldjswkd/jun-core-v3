#include "NetBase.h"
#include <mutex>

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


void NetBase::CleanupIOCP()
{
	// Resource 기반 핸들 해제
	DetachFromIOCP();
	
	// 레거시 핸들 초기화 (Resource가 실제 소유권 관리)
	h_iocp = INVALID_HANDLE_VALUE;
	ownsIOCP = false;

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

//------------------------------
// 새로운 Resource 기반 IOCP 인터페이스 구현
//------------------------------

void NetBase::AttachToIOCP(const IOCPResource& resource)
{
	// 기존 연결이 있으면 해제
	DetachFromIOCP();
	
	// 새로운 IOCPHandle 생성 및 연결
	iocpHandle.emplace(resource);
	
	// 레거시 핸들도 동기화 (호환성 유지)
	h_iocp = resource.GetHandle();
	ownsIOCP = false;  // Resource가 소유권을 가짐
}

void NetBase::DetachFromIOCP()
{
	// IOCPHandle 해제
	if (iocpHandle) {
		iocpHandle.reset();
	}
	
	// 호환성 핸들 초기화 (Resource가 소유권 관리하므로 항상 초기화)
	h_iocp = INVALID_HANDLE_VALUE;
	ownsIOCP = false;
}

bool NetBase::IsIOCPAttached() const noexcept
{
	// Resource 기반 핸들만 확인 (단일 IOCP 아키텍처)
	return iocpHandle && iocpHandle->IsValid();
}

//------------------------------
// 하위 호환성: 싱글톤 IOCP 관리
//------------------------------
void NetBase::EnsureSingletonIOCP()
{
	// 이미 연결되어 있으면 건너뛰기
	if (IsIOCPAttached()) {
		return;
	}
	
	// 싱글톤 IOCPResource 생성 및 연결
	static std::unique_ptr<IOCPResource> singletonIOCP = nullptr;
	static std::once_flag initFlag;
	
	// 해당 함수가 템플릿 함수가 되거나, 템플릿 클래스가 되면 안된다!! IOCPResource가 두개 이상 생성될 수 있다.
	 std::call_once(initFlag,
					[]() {
					singletonIOCP = IOCPResource::Builder()
					.WithWorkerCount(5)  // 기본 5개 워커 스레드
					.WithWorkerFunction([](HANDLE iocp) { NetBase::RunWorkerThread(iocp); })
					.Build();
		 });
	
	// 싱글톤 IOCP에 연결
	AttachToIOCP(*singletonIOCP);
}