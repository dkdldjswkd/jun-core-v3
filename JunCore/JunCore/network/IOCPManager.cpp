#include "IOCPManager.h"
#include "NetBase.h"
#include "Server.h"
#include "Client.h"
#include "../protocol/UnifiedPacketHeader.h"

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
            //LOG_DEBUG("recv size : %d", ioSize);
			HandleRecvComplete(session, ioSize);
		}
		// 송신 완료 처리  
		else if (p_overlapped == &session->send_overlapped_)
		{
            //LOG_DEBUG("send size : %d", ioSize);
			HandleSendComplete(session);
		}

	DecrementIOCount:
		session->DecrementIOCount();
	}
}

void IOCPManager::HandleRecvComplete(Session* session, DWORD ioSize)
{
	int loopCount = 0;
	const int MAX_LOOP_COUNT = 1000;

    // 수신 버퍼 업데이트
    session->recv_buf_.MoveRear(ioSize);
    session->last_recv_time_ = static_cast<DWORD>(GetTickCount64());

	for (;;)
	{
		// 무한루프 방지
		if (MAX_LOOP_COUNT < ++loopCount)
		{
            // 세션 잘라야하나?
			LOG_ERROR("IOCPManager: MAX_LOOP_COUNT reached");
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

		// 패킷 크기 유효성 검사
		if (!IsValidPacketSize(_packet_len))
		{
			LOG_ERROR("Invalid packet length: %d", _packet_len);
			return;
		}

		// 충분한 패킷 데이터가 도착했는지 확인
        if (static_cast<uint32_t>(_recv_byte) < _packet_len)
        {
            break;
        }

        // 패킷 추출
		std::vector<char> packet(_packet_len);
        session->recv_buf_.Dequeue(&packet[0], _packet_len);

        const UnifiedPacketHeader* header = reinterpret_cast<const UnifiedPacketHeader*>(&packet[0]);

        const uint32_t _packet_id = header->packet_id;
        //LOG_DEBUG("recv packet id : %d", _packet_id);

        // Payload 추출
        std::vector<char> payload(packet.begin() + UNIFIED_HEADER_SIZE, packet.end());

        NetBase* engine = session->GetEngine();
        if (engine) 
        {
            engine->OnPacketReceived(session, header->packet_id, payload);
        }
        else 
        {
            LOG_ERROR("Session has no engine assigned");
        }
	}

	if (!session->pending_disconnect_)
	{
		session->PostAsyncReceive();
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
    if (!session->pending_disconnect_ && session->send_q_.GetUseCount() > 0) 
    {
        session->PostAsyncSend();  // Session이 직접 처리
    }
}