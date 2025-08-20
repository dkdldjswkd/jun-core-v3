#pragma once
#include <Windows.h>
#include <thread>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// NetBase - NetServer와 NetClient의 공통 기반 클래스
//------------------------------
class NetBase
{
public:
	NetBase(const char* systemFile, const char* configSection);
	virtual ~NetBase();

protected:
	// 공통 열거형
	enum class NetType : BYTE {
		LAN,
		NET,
		NONE,
	};

	enum class PQCS_TYPE : BYTE {
		SEND_POST = 1,
		RELEASE_SESSION = 2,
		NONE,
	};

protected:
	// 프로토콜 관련 (완전히 동일)
	BYTE protocolCode;
	BYTE privateKey;
	NetType netType;

	// IOCP 관련 (완전히 동일)
	HANDLE h_iocp = INVALID_HANDLE_VALUE;

	// 모니터링 관련 (완전히 동일)
	DWORD recvMsgTPS = 0;
	DWORD sendMsgTPS = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

protected:
	// 공통 초기화 함수들
	virtual void InitializeProtocol(const char* systemFile, const char* configSection);
	virtual void InitializeIOCP(DWORD maxConcurrentThreads = 0);
	virtual void CleanupIOCP();

	// 공통 TPS 관리 함수들 (inline으로 성능 최적화)
	inline void UpdateBaseTPS() {
		sendMsgTPS = sendMsgCount;
		sendMsgCount = 0;
		recvMsgTPS = recvMsgCount;
		recvMsgCount = 0;
	}

	inline DWORD GetBaseSendTPS() const { return sendMsgTPS; }
	inline DWORD GetBaseRecvTPS() const { return recvMsgTPS; }

	// 공통 카운터 증가 함수들 (템플릿 콜백에서 사용)
	inline void IncrementSendCount(int count) { 
		InterlockedAdd((LONG*)&sendMsgCount, count); 
	}
	inline void IncrementRecvCount() { 
		InterlockedIncrement(&recvMsgCount); 
	}

protected:
	// 순수 가상 함수 - 파생 클래스에서 구현해야 함
	virtual void StartImpl() = 0;
	virtual void StopImpl() = 0;

public:
	// 공통 public 인터페이스
	void Start() { StartImpl(); }
	void Stop() { StopImpl(); }

	// 공통 TPS Getter (파생 클래스에서 확장 가능)
	virtual void UpdateTPS() { UpdateBaseTPS(); }
	DWORD GetSendTPS() const { return GetBaseSendTPS(); }
	DWORD GetRecvTPS() const { return GetBaseRecvTPS(); }
};

//------------------------------
// NetBase 인라인 함수 구현
//------------------------------

inline NetBase::NetBase(const char* systemFile, const char* configSection)
	: protocolCode(0), privateKey(0), netType(NetType::NONE)
{
	// WSAStartup을 먼저 호출 (소켓 함수 사용 전에 필수)
	WSADATA wsaData;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) 
	{
		throw std::exception("WSAStartup_ERROR");
	}
	
	InitializeProtocol(systemFile, configSection);
	// IOCP는 파생 클래스에서 activeWorker 설정 후 초기화
}

inline NetBase::~NetBase()
{
	CleanupIOCP();
}