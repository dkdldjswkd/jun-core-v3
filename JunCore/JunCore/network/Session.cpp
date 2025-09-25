#include "Session.h"
#include "NetBase.h"
#include <timeapi.h>
#include "../log.h"
#include "../protocol/UnifiedPacketHeader.h"


//------------------------------
// Session
//------------------------------

Session::Session() {}
Session::~Session() 
{
	if (engine_)
	{
		engine_->OnSessionDisconnect(this);
	}
	Release();
}

void Session::Set(SOCKET sock, in_addr ip, WORD port, NetBase* eng)
{
	sock_				= sock;
	ip_					= ip;
	port_				= port;
	send_flag_			= false;
	pending_disconnect_	= false;
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
	// 소켓 정리
	if (sock_ != INVALID_SOCKET) 
	{
		closesocket(sock_);
	}
	
	Set(INVALID_SOCKET, {0}, 0, nullptr);
	
	// Pool이 설정되어 있는 경우에만 반환
	if (session_pool_ && session_index_stack_)
	{
		// 자신의 인덱스 계산 (포인터 기반)
		for (DWORD i = 0; i < session_pool_->size(); ++i)
		{
			if ((*session_pool_)[i].get() == this)
			{
				// Pool에 인덱스 반환
				session_index_stack_->Push(i);
				break;
			}
		}
	}
}

void Session::SetSessionPool(std::vector<std::unique_ptr<Session>>* pool, LFStack<DWORD>* indexStack)
{
	session_pool_ = pool;
	session_index_stack_ = indexStack;
}


void Session::PostAsyncSend()
{
    WSABUF wsaBuf[MAX_SEND_MSG];
    int preparedCount = 0;

    if (MAX_SEND_MSG < send_q_.GetUseCount())
    {
        Disconnect();
        return;
    }

    for (int i = 0; i < MAX_SEND_MSG; i++)
    {
        if (send_q_.GetUseCount() <= 0) 
		{
            break;
        }

        send_q_.Dequeue(&send_packet_arr_[i]);
        ++preparedCount;

        wsaBuf[i].buf = send_packet_arr_[i]->data();
        wsaBuf[i].len = static_cast<DWORD>(send_packet_arr_[i]->size());
    }

    // 보낼 것이 없으면 실패
    if (preparedCount == 0) 
    {
        Disconnect();
        return;
    }

    // 준비된 패킷 수 커밋
    send_packet_count_ = preparedCount;

	// 추후 Pool 할당으로 개선
	auto send_overlapped_ = new OverlappedEx(shared_from_this(), IOOperation::IO_SEND);
    
    if (SOCKET_ERROR == WSASend(sock_, wsaBuf, send_packet_count_, NULL, 0, &send_overlapped_->overlapped_, NULL))
    {
        if (ERROR_IO_PENDING != WSAGetLastError())
        {
			// Worker에서 IOCount를 감소 시키기 위함
			PostQueuedCompletionStatus(h_iocp_, 1, (ULONG_PTR)this, (LPOVERLAPPED)1);  // PQCS::async_send_fail
        }
    }
}

bool Session::PostAsyncReceive()
{
	if (sock_ == INVALID_SOCKET || pending_disconnect_)
	{
		return false;
	}

    DWORD   flags = 0;
    WSABUF  wsaBuf[2];

    // 수신 쓰기 위치
    wsaBuf[0].buf = recv_buf_.GetWritePos();
    wsaBuf[0].len = recv_buf_.DirectEnqueueSize();
    // 수신 잔여 위치
    wsaBuf[1].buf = recv_buf_.GetBeginPos();
    wsaBuf[1].len = recv_buf_.RemainEnqueueSize();

	// 추후 Pool 할당으로 개선
	auto recv_overlapped = new OverlappedEx(shared_from_this(), IOOperation::IO_RECV);
    
	if (SOCKET_ERROR == WSARecv(sock_, wsaBuf, 2, NULL, &flags, &recv_overlapped->overlapped_, NULL))
    {
        if (ERROR_IO_PENDING != WSAGetLastError())
        {
            // Worker에서 IOCount를 감소 시키기 위함
			PostQueuedCompletionStatus(h_iocp_, 1, (ULONG_PTR)this, (LPOVERLAPPED)2);  // PQCS::async_recv_fail
            return false;
        }
    }

    return true;
}