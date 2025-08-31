#pragma once
#include <Windows.h>
#include "../buffer/packet.h"
#include "../../JunCommon/container/LFQueue.h"
#include "../../JunCommon/container/RingBuffer.h"
#include <vector>

constexpr int MAX_SEND_MSG = 100;
constexpr int INVALID_SESSION_ID = -1;

//------------------------------
// SessionId
//------------------------------
union SessionId
{
public:
	struct { DWORD index, unique; } s;
	DWORD64	sessionId = 0;
#define		SESSION_INDEX  s.index   
#define		SESSION_UNIQUE s.unique

public:
	SessionId();
	SessionId(DWORD64 value);
	SessionId(DWORD index, DWORD unique_no);
	~SessionId();

public:
	void operator=(const SessionId& other);
	void operator=(DWORD64 value);
	operator DWORD64();
};

//------------------------------
// Session
//------------------------------
class Session
{
public:
	Session();
	~Session();

public:
	// 소켓 정보
	SOCKET sock = INVALID_SOCKET;
	in_addr ip;
	WORD port;
	SessionId sessionId = INVALID_SESSION_ID;

	// flag
	bool sendFlag = false;
	bool disconnectFlag = false;

	// Send
	LFQueue<PacketBuffer*> sendQ;				// 송신 대기 큐
	PacketBuffer* sendPacketArr[MAX_SEND_MSG];	// 현재 전송중인 패킷 (Send 완료 통지까지 보관되어야한다.)
	LONG sendPacketCount = 0;					// 현재 전송중인 패킷 개수

	// Recv
	RingBuffer recvBuf;

	// TimeOut
	DWORD lastRecvTime;

	// Overlapped
	OVERLAPPED recvOverlapped = { 0, };
	OVERLAPPED sendOverlapped = { 0, };

	// 세션 해제여부와 카운트 관리 (releaseFlag, ioCount를 8byte로 정렬, false 캐시라인과 분리되게 유도)
	alignas(64) BOOL releaseFlag = true;
	LONG ioCount = 0;

	// 암호화
	std::vector<char> AESkey;
	unsigned long long send_seq_number_ = 0;

private:
	HANDLE h_iocp = INVALID_HANDLE_VALUE;  // IOCP 핸들 저장
	class NetBase* engine = nullptr;       // 이 세션을 소유한 엔진

public:
	void Set(SOCKET sock, in_addr ip, WORD port, SessionId sessionId);
	// Reset : ReleaseSession()
	
	// IOCP 핸들 설정 (초기화 시 호출)
	inline void SetIOCP(HANDLE iocp_handle) { h_iocp = iocp_handle; }
	
	// 엔진 설정 (초기화 시 호출)
	inline void SetEngine(class NetBase* eng) { engine = eng; }
	inline class NetBase* GetEngine() const { return engine; }
	
	// IO Count 관리 함수들
	inline void IncrementIOCount() noexcept
	{
		InterlockedIncrement(&ioCount);
	}
	
	inline bool DecrementIOCount() noexcept
	{
		return (0 == InterlockedDecrement(&ioCount));
	}
	
	inline bool DecrementIOCountPQCS() noexcept
	{
		if (0 == InterlockedDecrement(&ioCount)) 
		{
			PostQueuedCompletionStatus(h_iocp, 1, (ULONG_PTR)this, (LPOVERLAPPED)2); // RELEASE_SESSION
			return true;
		}
		return false;
	}
	
	// 연결 해제
	inline void DisconnectSession() noexcept
	{
		disconnectFlag = true;
		CancelIoEx((HANDLE)sock, NULL);
	}
};
typedef Session* PSession;