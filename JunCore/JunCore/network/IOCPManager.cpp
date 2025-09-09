#include "IOCPManager.h"
#include "NetBase.h"
#include "../protocol/UnifiedPacketHeader.h"
#include "../protocol/DirectProtobuf.h"

//------------------------------
// IOCPManager 구현 - 패킷 조립 로직
//------------------------------

void IOCPManager::RunWorkerThread()
{
    for (;;) 
    {
        DWORD ioSize = 0;
        Session* session = nullptr;
        LPOVERLAPPED p_overlapped = nullptr;

        BOOL retGQCS = GetQueuedCompletionStatus(iocpHandle, &ioSize, (PULONG_PTR)&session, &p_overlapped, INFINITE);

        // IOCP 종료 메시지
        if (ioSize == 0 && session == nullptr && p_overlapped == nullptr)
        {
            // 다른 Worker에게도 종료를 알린다.
			PostQueuedCompletionStatus(iocpHandle, 0, 0, 0);
            break;
        }

        if (ioSize == 0)
        {
            goto DecrementIOCount;
        }

		if (PQCS::none < static_cast<PQCS>(reinterpret_cast<uintptr_t>(p_overlapped)) && static_cast<PQCS>(reinterpret_cast<uintptr_t>(p_overlapped)) < PQCS::max)
		{
			goto DecrementIOCount;
		}

		// 수신 완료 처리
		if (p_overlapped == &session->recv_overlapped_)
		{
            LOG_DEBUG("recv size : %d", ioSize);
			HandleRecvComplete(session, ioSize);
		}
		// 송신 완료 처리  
		else if (p_overlapped == &session->send_overlapped_)
		{
            LOG_DEBUG("send size : %d", ioSize);
			HandleSendComplete(session);
		}

	DecrementIOCount:
		session->DecrementIOCount();
	}
}

void IOCPManager::HandleRecvComplete(Session* session, DWORD ioSize)
{
	int loopCount = 0;
	const int MAX_LOOP_COUNT = 100;

    // 수신 버퍼 업데이트
    session->recv_buf_.MoveRear(ioSize);
    session->last_recv_time_ = static_cast<DWORD>(GetTickCount64());

	for (;;)
	{
		// 무한루프 방지
		if (MAX_LOOP_COUNT < ++loopCount)
		{
			LOG_WARN("IOCPManager: MAX_LOOP_COUNT reached");
			break;
		}

		const int _recv_byte = session->recv_buf_.GetUseSize();

		// 최소 데이터 수신 여부 체크 (UnifiedPacketHeader 크기)
		if (_recv_byte <= UNIFIED_HEADER_SIZE)
		{
			break; 
		}

		uint32_t _packet_len;
		session->recv_buf_.Peek(&_packet_len, sizeof(uint32_t));

		// 패킷 크기 유효성 검사 (UnifiedPacketHeader 기준)
		if (!IsValidPacketSize(_packet_len))
		{
			LOG_ERROR("Invalid packet length: %d", _packet_len);
			return;
		}

		// 충분한 패킷 데이터가 도착했는지 확인
        if (_recv_byte < _packet_len)
        {
            break;
        }

        // 패킷 추출 (UnifiedPacketHeader 사용)
		std::vector<char> packet(_packet_len);
        session->recv_buf_.Dequeue(&packet[0], _packet_len);

        const UnifiedPacketHeader* header = reinterpret_cast<const UnifiedPacketHeader*>(&packet[0]);

        const uint32_t _packet_id = header->packet_id;
        LOG_DEBUG("recv packet id : %d", _packet_id);

        // 패킷 Payload 추출 (헤더 이후 부분)
        std::vector<char> payload(packet.begin() + UNIFIED_HEADER_SIZE, packet.end());

        g_direct_packet_handler[header->packet_id](payload);
	}

	if (!session->disconnect_flag_)
	{
		PostAsyncReceive(session);
	}
}

void IOCPManager::HandleSendComplete(Session* session)
{
    // 송신 완료 처리 (vector<char> 해제)
    for (int i = 0; i < session->send_packet_count_; i++) 
    {
        delete session->send_packet_arr_[i];  // vector<char> 메모리 해제
        session->send_packet_arr_[i] = nullptr;
    }
    session->send_packet_count_ = 0;

    // Send Flag OFF
    InterlockedExchange8((char*)&session->send_flag_, false);

    // 추가 송신이 필요한지 확인
    if (!session->disconnect_flag_ && session->send_q_.GetUseCount() > 0) 
    {
        PostAsyncSend(session);
    }
}

void IOCPManager::HandleSessionDisconnect(Session* session)
{
	if (NetBase* engine = session->GetEngine())
	{
		engine->OnClientLeave(session);
	}

	session->DisconnectSession();
}

//------------------------------
// 비동기 I/O 등록 함수들 (NetBase에서 이동)
//------------------------------

bool IOCPManager::PostAsyncReceive(Session* session)
{
    DWORD   flags = 0;
    WSABUF  wsaBuf[2];

    // 수신 쓰기 위치
    wsaBuf[0].buf = session->recv_buf_.GetWritePos();
    wsaBuf[0].len = session->recv_buf_.DirectEnqueueSize();
    // 수신 잔여 위치
    wsaBuf[1].buf = session->recv_buf_.GetBeginPos();
    wsaBuf[1].len = session->recv_buf_.RemainEnqueueSize();

    session->IncrementIOCount();
    ZeroMemory(&session->recv_overlapped_, sizeof(session->recv_overlapped_));
    
    if (SOCKET_ERROR == WSARecv(session->sock_, wsaBuf, 2, NULL, &flags, &session->recv_overlapped_, NULL))
    {
        if (ERROR_IO_PENDING != WSAGetLastError())
        {
            // Worker에서 IOCount를 감소 시키기 위함
			PostQueuedCompletionStatus(session->GetIOCP(), 1, (ULONG_PTR)this, (LPOVERLAPPED)PQCS::async_recv_fail);
            return false;
        }
    }

    return true;
}

void IOCPManager::PostAsyncSend(Session* session)
{
    WSABUF wsaBuf[MAX_SEND_MSG];
    int preparedCount = 0;

    // 운영 정책: 한 번에 보낼 수 있는 한도를 넘어선 세션은 즉시 차단
    if (session->send_q_.GetUseCount() > MAX_SEND_MSG)
    {
        HandleSessionDisconnect(session);
        return;
    }

    // 송신할 패킷들 준비 (vector<char> 직접 전송)
    for (int i = 0; i < MAX_SEND_MSG; i++)
    {
        if (session->send_q_.GetUseCount() <= 0) {
            break;
        }

        session->send_q_.Dequeue(&session->send_packet_arr_[i]);
        ++preparedCount;

        // 직접 raw 데이터 전송 (PacketBuffer 없이)
        wsaBuf[i].buf = session->send_packet_arr_[i]->data();
        wsaBuf[i].len = static_cast<DWORD>(session->send_packet_arr_[i]->size());
        
        LOG_DEBUG("[IOCP_SEND] Packet %d size: %d bytes", i, wsaBuf[i].len);
    }

    // 보낼 것이 없으면 실패
    if (preparedCount == 0) {
        HandleSessionDisconnect(session);
        return;
    }

    // 준비된 패킷 수 커밋
    session->send_packet_count_ = preparedCount;

    session->IncrementIOCount();
    ZeroMemory(&session->send_overlapped_, sizeof(session->send_overlapped_));
    
    if (SOCKET_ERROR == WSASend(session->sock_, wsaBuf, session->send_packet_count_, NULL, 0, &session->send_overlapped_, NULL))
    {
        if (ERROR_IO_PENDING != WSAGetLastError())
        {
			// Worker에서 IOCount를 감소 시키기 위함
			PostQueuedCompletionStatus(session->GetIOCP(), 1, (ULONG_PTR)this, (LPOVERLAPPED)PQCS::async_recv_fail);
        }
    }
}