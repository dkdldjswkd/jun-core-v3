#include "IOCPManager.h"
#include "IPacketHandler.h"
#include "NetBase_New.h"
#include "../protocol/message.h"

//------------------------------
// IOCPManager 구현 - 아름다운 패킷 조립 로직
//------------------------------

void IOCPManager::RunWorkerThread()
{
    // Thread-local 핸들러 등록 (향후 확장용)
    // PacketHandlerRegistry::RegisterHandler(someHandler);
    
    for (;;) 
    {
        DWORD ioSize = 0;
        Session* session = nullptr;
        LPOVERLAPPED p_overlapped = nullptr;

        BOOL retGQCS = GetQueuedCompletionStatus(iocpHandle, &ioSize, (PULONG_PTR)&session, &p_overlapped, INFINITE);

        // 워커 스레드 종료 신호
        if (shutdown.load() || (ioSize == 0 && session == nullptr && p_overlapped == nullptr)) 
        {
            // 다른 워커들에게도 종료 신호 전파
            if (!shutdown.load()) {
                PostQueuedCompletionStatus(iocpHandle, 0, 0, 0);
            }
            break;
        }

        // 세션 유효성 검사
        if (session == nullptr) {
            continue;
        }

        // FIN 패킷 (정상 종료)
        if (ioSize == 0) 
        {
            HandleSessionDisconnect(session);
            continue;
        }

        // PQCS (PostQueuedCompletionStatus) 처리
        if ((ULONG_PTR)p_overlapped < 3) 
        {
            // 향후 확장용 PQCS 타입들
            continue;
        }

        // 수신 완료 처리
        if (p_overlapped == &session->recvOverlapped) 
        {
            if (retGQCS) 
            {
                HandleRecvComplete(session, ioSize);
            }
            
            // IO Count 감소
            if (session->DecrementIOCount()) {
                // 세션이 해제된 경우 추가 처리 없음
                continue;
            }
        }
        // 송신 완료 처리  
        else if (p_overlapped == &session->sendOverlapped)
        {
            if (retGQCS) 
            {
                HandleSendComplete(session);
            }
            
            // IO Count 감소
            if (session->DecrementIOCount()) {
                // 세션이 해제된 경우 추가 처리 없음
                continue;
            }
        }
    }
    
    // Thread-local 핸들러 해제
    PacketHandlerRegistry::UnregisterHandler();
}

void IOCPManager::HandleRecvComplete(Session* session, DWORD ioSize)
{
    printf("📥 [IOCPManager] HandleRecvComplete - SessionID: %lld, ioSize: %lu\n", 
           session->sessionId.sessionId, ioSize);
    
    // 수신 버퍼 업데이트
    session->recvBuf.MoveRear(ioSize);
    session->lastRecvTime = static_cast<DWORD>(GetTickCount64());
    
    printf("📦 [IOCPManager] Buffer updated, starting packet assembly...\n");
    
    // 패킷 조립 (LAN 모드로 통일!)
    AssembleLANPackets(session);
}

void IOCPManager::AssembleLANPackets(Session* session)
{
    printf("🔧 [IOCPManager] AssembleLANPackets - Buffer size: %d\n", session->recvBuf.GetUseSize());
    
    int loopCount = 0;
    const int MAX_LOOP_COUNT = 100;  // 무한루프 방지
    
    for (;;)
    {
        // 무한루프 방지
        if (++loopCount > MAX_LOOP_COUNT) {
            printf("⚠️ [IOCPManager] MAX_LOOP_COUNT reached\n");
            break;
        }
        
        // 헤더 크기 확인
        if (session->recvBuf.GetUseSize() < LAN_HEADER_SIZE) {
            printf("📏 [IOCPManager] Not enough data for header: %d < %d\n", 
                   session->recvBuf.GetUseSize(), LAN_HEADER_SIZE);
            break;
        }

        // LAN 헤더 읽기 (2바이트 길이만)
        WORD payloadLen;
        session->recvBuf.Peek(&payloadLen, LAN_HEADER_SIZE);
        
        printf("📋 [IOCPManager] Header read - payloadLen: %d\n", payloadLen);

        // 패킷 크기 유효성 검사
        if (payloadLen > MAX_PAYLOAD_LEN || payloadLen == 0) 
        {
            printf("❌ [IOCPManager] Invalid payload length: %d\n", payloadLen);
            HandleSessionDisconnect(session);
            return;
        }

        // 전체 패킷이 도착했는지 확인
        if (session->recvBuf.GetUseSize() < LAN_HEADER_SIZE + payloadLen) {
            printf("⏳ [IOCPManager] Waiting for complete packet: %d < %d\n", 
                   session->recvBuf.GetUseSize(), LAN_HEADER_SIZE + payloadLen);
            break;
        }

        // 헤더 제거
        session->recvBuf.MoveFront(LAN_HEADER_SIZE);
        printf("✂️ [IOCPManager] Header removed, assembling packet...\n");

        // 🎨 아름다운 패킷 조립
        PacketBuffer* packet = PacketBuffer::Alloc();
        
        // 임시 버퍼를 통한 안전한 복사
        char tempBuffer[MAX_PAYLOAD_LEN];
        session->recvBuf.Dequeue(tempBuffer, payloadLen);
        packet->PutData(tempBuffer, payloadLen);

        printf("📦 [IOCPManager] Packet assembled successfully - Length: %d\n", payloadLen);

        // 완성된 패킷을 핸들러에게 전달
        DeliverPacketToHandler(session, packet);
    }

    // 계속 수신 등록 (세션이 살아있다면)
    if (!session->disconnectFlag) {
        // 수신 재등록은 NetBase에서 처리하던 것을 여기서 직접 처리
        PostAsyncReceive(session);
    }
}

void IOCPManager::HandleSendComplete(Session* session)
{
    // 송신 완료 처리
    for (int i = 0; i < session->sendPacketCount; i++) {
        PacketBuffer::Free(session->sendPacketArr[i]);
    }
    session->sendPacketCount = 0;

    // Send Flag OFF
    InterlockedExchange8((char*)&session->sendFlag, false);

    // 추가 송신이 필요한지 확인
    if (!session->disconnectFlag && session->sendQ.GetUseCount() > 0) {
        // 송신 재등록은 NetBase에서 처리하던 것을 여기서 직접 처리
        PostAsyncSend(session);
    }
}

void IOCPManager::HandleSessionDisconnect(Session* session)
{
    // 세션 연결 해제 처리
    session->DisconnectSession();
    
    // 핸들러에게 연결 해제 알림
    IPacketHandler* handler = PacketHandlerRegistry::GetCurrentHandler();
    if (handler) {
        handler->OnSessionDisconnected(session);
    }
}

void IOCPManager::DeliverPacketToHandler(Session* session, PacketBuffer* packet)
{
    // 세션에서 직접 엔진 가져와서 호출 (Thread-local 방식 대신)
    NetBase* engine = session->GetEngine();
    if (engine && packet) {
        printf("🚀 [IOCPManager] Delivering packet to handler (SessionID: %lld)\n", session->sessionId.sessionId);
        engine->HandleCompletePacket(session, packet);
    } else {
        printf("❌ [IOCPManager] No engine or invalid packet - freeing memory\n");
        if (packet) {
            PacketBuffer::Free(packet);
        }
    }
}

void IOCPManager::StartAsyncReceive(Session* session)
{
    // 첫 번째 비동기 수신 시작 (ServerSocketManager에서 호출)
    PostAsyncReceive(session);
}

//------------------------------
// 비동기 I/O 등록 함수들 (NetBase에서 이동)
//------------------------------

void IOCPManager::PostAsyncReceive(Session* session)
{
    DWORD flags = 0;
    WSABUF wsaBuf[2];

    // 수신 쓰기 위치
    wsaBuf[0].buf = session->recvBuf.GetWritePos();
    wsaBuf[0].len = session->recvBuf.DirectEnqueueSize();
    // 수신 잔여 위치
    wsaBuf[1].buf = session->recvBuf.GetBeginPos();
    wsaBuf[1].len = session->recvBuf.RemainEnqueueSize();

    session->IncrementIOCount();
    ZeroMemory(&session->recvOverlapped, sizeof(session->recvOverlapped));
    
    if (SOCKET_ERROR == WSARecv(session->sock, wsaBuf, 2, NULL, &flags, &session->recvOverlapped, NULL))
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            // 즉시 실패 시 세션 해제
            HandleSessionDisconnect(session);
            session->DecrementIOCount();
            return;
        }
    }

    // 이미 끊기기로 한 세션이면 등록을 취소
    if (session->disconnectFlag)
    {
        CancelIoEx((HANDLE)session->sock, NULL);
    }
}

void IOCPManager::PostAsyncSend(Session* session)
{
    WSABUF wsaBuf[MAX_SEND_MSG];
    int preparedCount = 0;

    // 운영 정책: 한 번에 보낼 수 있는 한도를 넘어선 세션은 즉시 차단
    if (session->sendQ.GetUseCount() > MAX_SEND_MSG)
    {
        HandleSessionDisconnect(session);
        return;
    }

    // 송신할 패킷들 준비
    for (int i = 0; i < MAX_SEND_MSG; i++)
    {
        if (session->sendQ.GetUseCount() <= 0) {
            break;
        }

        session->sendQ.Dequeue((PacketBuffer**)&session->sendPacketArr[i]);
        ++preparedCount;

        // LAN 모드로 통일
        wsaBuf[i].buf = session->sendPacketArr[i]->GetLanPacketPos();
        wsaBuf[i].len = session->sendPacketArr[i]->GetLanPacketSize();
    }

    // 보낼 것이 없으면 실패
    if (preparedCount == 0) {
        HandleSessionDisconnect(session);
        return;
    }

    // 준비된 패킷 수 커밋
    session->sendPacketCount = preparedCount;

    session->IncrementIOCount();
    ZeroMemory(&session->sendOverlapped, sizeof(session->sendOverlapped));
    
    if (SOCKET_ERROR == WSASend(session->sock, wsaBuf, session->sendPacketCount, NULL, 0, &session->sendOverlapped, NULL))
    {
        const auto err_no = WSAGetLastError();
        if (ERROR_IO_PENDING != err_no)
        {
            // 즉시 실패 시 세션 해제
            HandleSessionDisconnect(session);
            session->DecrementIOCount();
        }
    }
}