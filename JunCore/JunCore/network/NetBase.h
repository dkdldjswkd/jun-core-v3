#pragma once
#include <Windows.h>
#include <WinSock2.h>
#include <thread>
#include <functional>
#include "Session.h"
#include "../buffer/packet.h"
#include "../../JunCommon/algorithm/Parser.h"

//------------------------------
// NetBase - NetworkEngine<ServerPolicy>와 NetworkEngine<ClientPolicy>의 공통 기반 클래스
//------------------------------
class NetBase
{
	friend struct ServerPolicy;
	friend struct ClientPolicy;
	
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

	inline DWORD GetBaseSendTPS() const noexcept { return sendMsgTPS; }
	inline DWORD GetBaseRecvTPS() const noexcept { return recvMsgTPS; }

	// 공통 카운터 증가 함수들 (템플릿 콜백에서 사용)
	inline void IncrementSendCount(int count) { 
		InterlockedAdd((LONG*)&sendMsgCount, count); 
	}
	inline void IncrementRecvCount() { 
		InterlockedIncrement(&recvMsgCount); 
	}

protected:
	// IOCP 비동기 I/O 함수들
	template<typename OnErrorFn>
	bool PostReceive(Session& session, OnErrorFn&& onError);
	
	template<typename OnDisconnectFn, typename OnErrorFn>
	bool PostSend(Session& session, bool isLAN, OnDisconnectFn&& onDisconnect, OnErrorFn&& onError);
	
	// 완료 처리 함수들
	template<typename OnSendPostFn, typename OnIncrementCountFn>
	void OnSendComplete(Session& session, OnSendPostFn&& onSendPost, OnIncrementCountFn&& onIncrementCount);
	
	template<typename OnRecvFn, typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
	void OnReceiveCompleteLAN(Session& session, OnRecvFn&& onRecv, OnDisconnectFn&& onDisconnect, 
							  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount);
	
	template<typename OnRecvFn, typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
	void OnReceiveCompleteNET(Session& session, OnRecvFn&& onRecv, OnDisconnectFn&& onDisconnect, 
							  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount);
	
	// 워커 스레드 메인 루프
	template<typename SessionManagerT>
	void RunWorkerThread(SessionManagerT& sessionManager);

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

//------------------------------
// NetBase 템플릿 함수 구현
//------------------------------

// PostReceive - 비동기 수신 등록(WSARecv)
template<typename OnErrorFn>
inline bool NetBase::PostReceive(Session& session, OnErrorFn&& onError)
{
	DWORD flags = 0;
	WSABUF wsaBuf[2];

	// 수신 쓰기 위치
	wsaBuf[0].buf = session.recvBuf.GetWritePos();
	wsaBuf[0].len = session.recvBuf.DirectEnqueueSize();
	// 수신 잔여 위치
	wsaBuf[1].buf = session.recvBuf.GetBeginPos();
	wsaBuf[1].len = session.recvBuf.RemainEnqueueSize();

	InterlockedIncrement((LONG*)&session.ioCount);
	ZeroMemory(&session.recvOverlapped, sizeof(session.recvOverlapped));
	if (SOCKET_ERROR == WSARecv(session.sock, wsaBuf, 2, NULL, &flags, &session.recvOverlapped, NULL))
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			// 제출 단계에서 즉시 실패한 경우
			onError(&session);
			return false;
		}
	}

	// 이미 끊기기로 한 세션이면 등록을 취소
	if (session.disconnectFlag)
	{
		CancelIoEx((HANDLE)session.sock, NULL);
		return false;
	}
	return true;
}

// PostSend - 비동기 송신 등록(WSASend)
template<typename OnDisconnectFn, typename OnErrorFn>
inline bool NetBase::PostSend(Session& session, bool isLAN, OnDisconnectFn&& onDisconnect, OnErrorFn&& onError)
{
	WSABUF wsaBuf[MAX_SEND_MSG];
	int preparedCount = 0;

	// 운영 정책: 한 번에 보낼 수 있는 한도를 넘어설 정도로 큐가 불어난 세션은 즉시 차단
	if (session.sendQ.GetUseCount() > MAX_SEND_MSG)
	{
		onDisconnect(&session);
		return false;
	}

	// 한 번의 WSASend에 담을 패킷을 준비
	for (int i = 0; i < MAX_SEND_MSG; i++)
	{
		if (session.sendQ.GetUseCount() <= 0)
			break;

		session.sendQ.Dequeue((PacketBuffer**)&session.sendPacketArr[i]);
		++preparedCount;

		if (isLAN)
		{
			wsaBuf[i].buf = session.sendPacketArr[i]->GetLanPacketPos();
			wsaBuf[i].len = session.sendPacketArr[i]->GetLanPacketSize();
		}
		else
		{
			wsaBuf[i].buf = session.sendPacketArr[i]->GetNetPacketPos();
			wsaBuf[i].len = session.sendPacketArr[i]->GetNetPacketSize();
		}
	}

	// 보낼 것이 없으면 실패로 간주
	if (preparedCount == 0)
	{
		onDisconnect(&session);
		return false;
	}

	// 제출 직전 커밋(완료 콜백에서 preparedCount만큼만 해제/집계)
	session.sendPacketCount = preparedCount;

	InterlockedIncrement((LONG*)&session.ioCount);
	ZeroMemory(&session.sendOverlapped, sizeof(session.sendOverlapped));
	if (SOCKET_ERROR == WSASend(session.sock, wsaBuf, session.sendPacketCount, NULL, 0, &session.sendOverlapped, NULL))
	{
		const auto err_no = WSAGetLastError();
		if (ERROR_IO_PENDING != err_no)
		{
			// 제출 단계에서 즉시 실패
			onDisconnect(&session);
			onError(&session);
			return false;
		}
	}
	return true;
}

// OnSendComplete - Send 완료 처리
template<typename OnSendPostFn, typename OnIncrementCountFn>
inline void NetBase::OnSendComplete(Session& session, OnSendPostFn&& onSendPost, OnIncrementCountFn&& onIncrementCount)
{
	// Send Packet Free
	for (int i = 0; i < session.sendPacketCount; i++)
	{
		PacketBuffer::Free(session.sendPacketArr[i]);
	}

	// 통계 업데이트 (NetworkEngine<ServerPolicy> 방식: 배치 처리가 더 효율적)
	onIncrementCount(session.sendPacketCount);
	session.sendPacketCount = 0;

	// Send Flag OFF
	InterlockedExchange8((char*)&session.sendFlag, false);

	// Send 재전송 체크
	if (session.disconnectFlag || session.sendQ.GetUseCount() <= 0)
	{
		return;
	}

	onSendPost();
}

// OnReceiveCompleteLAN - Recv 완료 처리 (LAN 모드)
template<typename OnRecvFn, typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
inline void NetBase::OnReceiveCompleteLAN(Session& session, OnRecvFn&& onRecv, OnDisconnectFn&& onDisconnect, 
										  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount)
{
	for (;;)
	{
		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE)
			break;

		LanHeader lanHeader;
		session.recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		if (lanHeader.len > MAX_PAYLOAD_LEN)
		{
			onDisconnect();
			return;
		}

		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE + lanHeader.len)
			break;

		session.recvBuf.MoveFront(LAN_HEADER_SIZE);

		PacketBuffer* contentsPacket = PacketBuffer::Alloc();
		char tempBuffer[MAX_PAYLOAD_LEN];
		session.recvBuf.Dequeue(tempBuffer, lanHeader.len);
		contentsPacket->PutData(tempBuffer, lanHeader.len);

		onRecv(contentsPacket);
		onIncrementCount();
	}

	if (!session.disconnectFlag)
	{
		onAsyncRecv();
	}
}

// OnReceiveCompleteNET - Recv 완료 처리 (NET 모드)
template<typename OnRecvFn, typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
inline void NetBase::OnReceiveCompleteNET(Session& session, OnRecvFn&& onRecv, OnDisconnectFn&& onDisconnect, 
										  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount)
{
	for (;;)
	{
		if (session.recvBuf.GetUseSize() < NET_HEADER_SIZE)
			break;

		NetHeader netHeader;
		session.recvBuf.Peek(&netHeader, NET_HEADER_SIZE);

		if (netHeader.code != protocolCode)
		{
			onDisconnect();
			return;
		}

		if (netHeader.len > MAX_PAYLOAD_LEN)
		{
			onDisconnect();
			return;
		}

		if (session.recvBuf.GetUseSize() < NET_HEADER_SIZE + netHeader.len)
			break;

		// 암호화 패킷 준비
		char encryptPacket[NET_HEADER_SIZE + MAX_PAYLOAD_LEN];
		session.recvBuf.Peek(encryptPacket, NET_HEADER_SIZE + netHeader.len);
		session.recvBuf.MoveFront(NET_HEADER_SIZE + netHeader.len);

		// 복호화
		PacketBuffer* decryptPacket = PacketBuffer::Alloc();
		if (false == decryptPacket->DecryptPacket(encryptPacket, privateKey))
		{
			PacketBuffer::Free(decryptPacket);
			onDisconnect();
			return;
		}

		onRecv(decryptPacket);
		onIncrementCount();
	}

	if (!session.disconnectFlag)
	{
		onAsyncRecv();
	}
}

// RunWorkerThread - IOCP Worker 함수 공통 로직
template<typename SessionManagerT>
inline void NetBase::RunWorkerThread(SessionManagerT& sessionManager)
{
	for (;;) 
	{
		DWORD ioSize = 0;
		Session* session = nullptr;
		LPOVERLAPPED p_overlapped = nullptr;

		BOOL retGQCS = GetQueuedCompletionStatus(h_iocp, &ioSize, (PULONG_PTR)&session, &p_overlapped, INFINITE);

		// 워커 스레드 종료
		if (ioSize == 0 && session == nullptr && p_overlapped == nullptr) 
		{
			PostQueuedCompletionStatus(h_iocp, 0, 0, 0);
			return;
		}

		// FIN
		if (ioSize == 0) 
		{
			goto Decrement_IOCount;
		}
		// PQCS
		else if ((ULONG_PTR)p_overlapped < 3) // PQCS_TYPE::NONE == 3
		{
			int pqcsType = (int)(ULONG_PTR)p_overlapped;
			switch (pqcsType) 
			{
				case 1: // SEND_POST
				{
					sessionManager.HandleSendPost(session);
					goto Decrement_IOCount;
				}
				case 2: // RELEASE_SESSION
				{
					sessionManager.HandleReleaseSession(session);
					continue;
				}
			}
		}
		// recv 완료처리
		else if (&session->recvOverlapped == p_overlapped) 
		{
			if (retGQCS) 
			{
				session->recvBuf.MoveRear(ioSize);
				session->lastRecvTime = static_cast<DWORD>(GetTickCount64());
				sessionManager.HandleRecvCompletion(session);
			}
		}
		// send 완료처리
		else if (&session->sendOverlapped == p_overlapped) 
		{
			if (retGQCS) 
			{
				sessionManager.HandleSendCompletion(session);
			}
		}

	Decrement_IOCount:
		sessionManager.HandleDecrementIOCount(session);
	}
}