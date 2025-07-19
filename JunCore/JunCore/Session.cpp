#include "Session.h"
#include <timeapi.h>

//------------------------------
// SESSION ID
//------------------------------

SessionId::SessionId() {}
SessionId::SessionId(DWORD64 value) {
  sessionId = value;
}
SessionId::SessionId(DWORD index, DWORD unique_no) {
  SESSION_INDEX = index;
  SESSION_UNIQUE = unique_no;
}
SessionId::~SessionId() {}

void SessionId::operator=(const SessionId& other) {
	sessionId = other.sessionId;
}

void SessionId::operator=(DWORD64 value) {
	sessionId = value;
}

SessionId::operator DWORD64() {
	return sessionId;
}

//------------------------------
// Session
//------------------------------

Session::Session() {}
Session::~Session() {}

void Session::Set(SOCKET sock, in_addr ip, WORD port, SessionId sessionId) {
	this->sock = sock;
	this->ip = ip;
	this->port = port;
	this->sessionId = sessionId;
	recvBuf.Clear();
	sendFlag = false;
	disconnectFlag = false;
	sendPacketCount = 0;
	lastRecvTime = timeGetTime();

	// �������� ���� ������ �Ǵ°��� ����
	InterlockedIncrement(&ioCount);
	this->releaseFlag = false;
}