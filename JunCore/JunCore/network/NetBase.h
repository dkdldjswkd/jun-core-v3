#pragma once
#include "../core/WindowsIncludes.h"
#include <thread>
#include <functional>
#include <optional>
#include "Session.h"
#include "../buffer/packet.h"
#include "IOCPResource.h"
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

	// IOCP 관련 - Resource 기반 인터페이스
	std::optional<IOCPHandle> iocpHandle;  // Resource 기반 핸들
	
	// 호환성 유지용 핸들 (실제 소유권은 IOCPResource가 관리)
	HANDLE h_iocp = INVALID_HANDLE_VALUE;
	bool ownsIOCP = false;  // 항상 false (Resource가 소유권 관리)

	// 모니터링 관련 (완전히 동일)
	DWORD recvMsgTPS = 0;
	DWORD sendMsgTPS = 0;
	alignas(64) DWORD recvMsgCount = 0;
	alignas(64) DWORD sendMsgCount = 0;

protected:
	// 공통 초기화 함수들
	virtual void InitializeProtocol(const char* systemFile, const char* configSection);
	
public:
	//------------------------------
	// Resource 기반 IOCP 인터페이스
	//------------------------------
	void AttachToIOCP(const IOCPResource& resource);
	void DetachFromIOCP();
	bool IsIOCPAttached() const noexcept;
	
	// 하위 호환성: 싱글톤 IOCP 자동 연결
	void EnsureSingletonIOCP();

protected:
	
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
	
	template<typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
	void OnReceiveCompleteLAN(Session& session, OnDisconnectFn&& onDisconnect, 
							  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount);
	
	template<typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
	void OnReceiveCompleteNET(Session& session, OnDisconnectFn&& onDisconnect, 
							  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount);
	
public:
	// IOCP 워커 스레드 (CompletionKey 라우팅) - NetworkManager에서 사용
	static void RunWorkerThread(HANDLE iocpHandle);

protected:
	
	// IOCP에서 호출될 가상 핸들러들
	virtual void HandleRecvComplete(Session* session) = 0;
	virtual void HandleSendComplete(Session* session) = 0;
	virtual void HandleSessionDisconnect(Session* session) = 0;
	virtual void HandleDecrementIOCount(Session* session) = 0;
	
	// PacketJob 제출 인터페이스 (파생 클래스에서 NetworkManager에 연결)
	virtual void SubmitPacketJob(Session* session, PacketBuffer* packet) = 0;

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
	int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (0 != wsaResult) 
	{
		printf("!!! WSAStartup failed with error: %d !!!\n", wsaResult);
		fflush(stdout);
		throw std::exception("WSAStartup_ERROR");
	}
	printf("WSAStartup successful\n");
	fflush(stdout);
	
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
// IOCP Worker Thread에서 호출 - 패킷 조립만 담당, 핸들링은 HandlerPool로 위임
template<typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
inline void NetBase::OnReceiveCompleteLAN(Session& session, OnDisconnectFn&& onDisconnect, 
										  OnAsyncRecvFn&& onAsyncRecv, OnIncrementCountFn&& onIncrementCount)
{
	int loopCount = 0;
	const int MAX_LOOP_COUNT = 100;  // 무한루프 방지
	
	for (;;)
	{
		// 무한루프 방지
		if (++loopCount > MAX_LOOP_COUNT) {
			// printf 제거 - 크래시 원인 가능성
			break;
		}
		
		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE)
			break;

		LanHeader lanHeader;
		session.recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		if (lanHeader.len > MAX_PAYLOAD_LEN)
		{
			onDisconnect();
			return;
		}
		
		if (lanHeader.len == 0) {
			onDisconnect();
			return;
		}

		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE + lanHeader.len)
			break;

		session.recvBuf.MoveFront(LAN_HEADER_SIZE);

		// 패킷 조립만 담당 - 실제 처리는 HandlerPool로 위임
		PacketBuffer* contentsPacket = PacketBuffer::Alloc();
		char tempBuffer[MAX_PAYLOAD_LEN];
		session.recvBuf.Dequeue(tempBuffer, lanHeader.len);
		contentsPacket->PutData(tempBuffer, lanHeader.len);

		// HandlerPool로 PacketJob 제출 (IOCP Worker는 패킷 조립만!)
		SubmitPacketJob(&session, contentsPacket);
		onIncrementCount();
	}

	if (!session.disconnectFlag)
	{
		onAsyncRecv();
	}
}

// OnReceiveCompleteNET - Recv 완료 처리 (NET 모드)
// IOCP Worker Thread에서 호출 - 패킷 조립만 담당, 핸들링은 HandlerPool로 위임
template<typename OnDisconnectFn, typename OnAsyncRecvFn, typename OnIncrementCountFn>
inline void NetBase::OnReceiveCompleteNET(Session& session, OnDisconnectFn&& onDisconnect, 
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

		// 복호화 (패킷 조립)
		PacketBuffer* decryptPacket = PacketBuffer::Alloc();
		if (false == decryptPacket->DecryptPacket(encryptPacket, privateKey))
		{
			PacketBuffer::Free(decryptPacket);
			onDisconnect();
			return;
		}

		// HandlerPool로 PacketJob 제출 (IOCP Worker는 패킷 조립만!)
		SubmitPacketJob(&session, decryptPacket);
		onIncrementCount();
	}

	if (!session.disconnectFlag)
	{
		onAsyncRecv();
	}
}