#include "Session.h"
#include <timeapi.h>

//------------------------------
// SESSION ID
//------------------------------

SessionId::SessionId() {}
SessionId::SessionId(DWORD64 value) 
{
  sessionId = value;
}
SessionId::SessionId(DWORD index, DWORD unique_no) 
{
  SESSION_INDEX = index;
  SESSION_UNIQUE = unique_no;
}
SessionId::~SessionId() {}

void SessionId::operator=(const SessionId& other) 
{
	sessionId = other.sessionId;
}

void SessionId::operator=(DWORD64 value) 
{
	sessionId = value;
}

SessionId::operator DWORD64() 
{
	return sessionId;
}

//------------------------------
// Session
//------------------------------

Session::Session() {}
Session::~Session() {}

void Session::Set(SOCKET sock, in_addr ip, WORD port, SessionId session_id, NetBase* eng)
{
	release_flag_		= false;
	sock_				= sock;
	ip_					= ip;
	port_				= port;
	session_id_			= session_id;
	send_flag_			= false;
	disconnect_flag_	= false;
	send_packet_count_	= 0;
	last_recv_time_		= static_cast<DWORD>(GetTickCount64());
	engine_				= eng;

	recv_buf_.Clear();
	PacketBuffer* packet;
	while (send_q_.Dequeue(&packet)) 
	{
		PacketBuffer::Free(packet);
	}
}

void Session::Release()
{
	// 소켓 정리
	if (sock_ != INVALID_SOCKET) 
	{
		closesocket(sock_);
	}
	
	Set(INVALID_SOCKET, {0}, 0, SessionId(0, 0), nullptr);
	
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