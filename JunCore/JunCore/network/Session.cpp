#include "Session.h"
#include <timeapi.h>
#include "../log.h"
#include "../protocol/UnifiedPacketHeader.h"


//------------------------------
// Session
//------------------------------

Session::Session() {}
Session::~Session() {}

void Session::Set(SOCKET sock, in_addr ip, WORD port, NetBase* eng)
{
	release_flag_		= false;
	sock_				= sock;
	ip_					= ip;
	port_				= port;
	send_flag_			= false;
	disconnect_flag_	= false;
	send_packet_count_	= 0;
	last_recv_time_		= static_cast<DWORD>(GetTickCount64());
	engine_				= eng;

	recv_buf_.Clear();
	std::vector<char>* packet_data;
	while (send_q_.Dequeue(&packet_data)) 
	{
		delete packet_data;  // vector<char> 메모리 해제
	}
}

void Session::Release()
{
	LOG_DEBUG("[Session Release] Session: 0x%llX", (uintptr_t)this);

	// 소켓 정리
	if (sock_ != INVALID_SOCKET) 
	{
		closesocket(sock_);
	}
	
	Set(INVALID_SOCKET, {0}, 0, nullptr);
	
	// 해제 상태로 설정
	release_flag_ = true;
	io_count_ = 0;
	
	// Pool이 설정되어 있는 경우에만 반환
	if (session_pool_ && session_index_stack_)
	{
		// 자신의 인덱스 계산
		DWORD index = (DWORD)(this - &(*session_pool_)[0]);

		// 인덱스 유효성 검사
		if (index < session_pool_->size())
		{
			// Pool에 인덱스 반환
			session_index_stack_->Push(index);
		}
	}
}

void Session::SetSessionPool(std::vector<Session>* pool, LFStack<DWORD>* indexStack)
{
	session_pool_ = pool;
	session_index_stack_ = indexStack;
}

void Session::PostAsyncSend()
{
    WSABUF wsaBuf[MAX_SEND_MSG];
    int preparedCount = 0;

    // 운영 정책: 한 번에 보낼 수 있는 한도를 넘어선 세션은 즉시 차단
    if (send_q_.GetUseCount() > MAX_SEND_MSG)
    {
        DisconnectSession();
        return;
    }

    // 송신할 패킷들 준비 (vector<char> 직접 전송)
    for (int i = 0; i < MAX_SEND_MSG; i++)
    {
        if (send_q_.GetUseCount() <= 0) {
            break;
        }

        send_q_.Dequeue(&send_packet_arr_[i]);
        ++preparedCount;

        // 직접 raw 데이터 전송 (PacketBuffer 없이)
        wsaBuf[i].buf = send_packet_arr_[i]->data();
        wsaBuf[i].len = static_cast<DWORD>(send_packet_arr_[i]->size());
        
        LOG_DEBUG("[SESSION_SEND] Packet %d size: %d bytes", i, wsaBuf[i].len);
    }

    // 보낼 것이 없으면 실패
    if (preparedCount == 0) 
    {
        DisconnectSession();
        return;
    }

    // 준비된 패킷 수 커밋
    send_packet_count_ = preparedCount;

    IncrementIOCount();
    ZeroMemory(&send_overlapped_, sizeof(send_overlapped_));
    
    if (SOCKET_ERROR == WSASend(sock_, wsaBuf, send_packet_count_, NULL, 0, &send_overlapped_, NULL))
    {
        if (ERROR_IO_PENDING != WSAGetLastError())
        {
			// Worker에서 IOCount를 감소 시키기 위함 (자신의 IOCP 핸들 사용)
			PostQueuedCompletionStatus(h_iocp_, 1, (ULONG_PTR)this, (LPOVERLAPPED)1);  // PQCS::async_send_fail
        }
    }
}