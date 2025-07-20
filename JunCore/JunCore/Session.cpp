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

void Session::Set(SOCKET sock, in_addr ip, WORD port, SessionId sessionId) 
{
	this->sock = sock;
	this->ip = ip;
	this->port = port;
	this->sessionId = sessionId;
	recvBuf.Clear();
	sendFlag = false;
	disconnectFlag = false;
	sendPacketCount = 0;
	lastRecvTime = timeGetTime();

	// 클라이언트의 접속 요청을 허용할지 여부
	InterlockedIncrement(&ioCount);
	this->releaseFlag = false;
}