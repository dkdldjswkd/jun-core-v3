#pragma once
#include <WinSock2.h>
#include "Session.h"

// 공통 I/O 헬퍼
// - NetClient/NetServer에서 겹치는 WSARecv/WSASend 준비 과정을 모아 둡니다.
// - 퍼블릭 API나 동작을 바꾸지 않고, 내부 중복만 줄이기 위한 용도입니다.

namespace IoCommon {

// 비동기 수신 등록(WSARecv)
// - 두 구간(직접 쓰기/랩 구간)을 WSABUF로 구성해 한 번에 등록합니다.
// - 호출 즉시 실패 시 decIo(session)로 IO 카운트를 원복합니다.
template <typename DecrementIoFn>
inline bool AsyncRecv(Session& session, DecrementIoFn&& decIo)
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
			decIo(&session);
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

// 비동기 송신 등록(WSASend)
// - 최대 MAX_SEND_MSG개까지 큐에서 꺼내 WSABUF로 구성합니다.
// - isLan에 따라 LAN/NET 헤더 기준으로 버퍼 위치를 선택합니다.
// - 제출 직전에만 session.sendPacketCount를 커밋하여 완료 콜백에서 정확히 해제/집계할 수 있게 합니다.
// - 제출 단계에서 즉시 실패하면 disconnect + decIo로 정리합니다.
template <typename DisconnectFn, typename DecrementIoFn>
inline bool AsyncSend(Session& session, bool isLan, DisconnectFn&& disconnect, DecrementIoFn&& decIo)
{
	WSABUF wsaBuf[MAX_SEND_MSG];
	int preparedCount = 0;

	// 운영 정책: 한 번에 보낼 수 있는 한도를 넘어설 정도로 큐가 불어난 세션은 즉시 차단
	if (session.sendQ.GetUseCount() > MAX_SEND_MSG)
	{
		disconnect(&session);
		return false;
	}

	// 한 번의 WSASend에 담을 패킷을 준비
	for (int i = 0; i < MAX_SEND_MSG; i++)
	{
		if (session.sendQ.GetUseCount() <= 0)
			break;

		session.sendQ.Dequeue((PacketBuffer**)&session.sendPacketArr[i]);
		++preparedCount;

		if (isLan)
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
		disconnect(&session);
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
			disconnect(&session);
			decIo(&session);
			return false;
		}
	}
	return true;
}

// Send 완료 처리
// - 전송된 패킷들을 해제하고 통계를 업데이트합니다.
// - sendFlag를 해제하고 조건에 맞으면 재전송을 시도합니다.
template <typename SendPostFn, typename IncrementCountFn>
inline void SendCompletion(Session& session, SendPostFn&& sendPost, IncrementCountFn&& incCount)
{
	// Send Packet Free
	for (int i = 0; i < session.sendPacketCount; i++)
	{
		PacketBuffer::Free(session.sendPacketArr[i]);
	}

	// 통계 업데이트 (NetServer 방식: 배치 처리가 더 효율적)
	incCount(session.sendPacketCount);
	session.sendPacketCount = 0;

	// Send Flag OFF
	InterlockedExchange8((char*)&session.sendFlag, false);

	// Send 재전송 체크
	if (session.disconnectFlag || session.sendQ.GetUseCount() <= 0)
	{
		return;
	}

	sendPost();
}

// Recv 완료 처리 - LAN 모드
// - LAN 헤더 기반 패킷을 파싱하고 OnRecv 콜백을 호출합니다.
template <typename OnRecvFn, typename DisconnectFn, typename AsyncRecvFn, typename IncrementCountFn>
inline void RecvCompletionLan(Session& session, OnRecvFn&& onRecv, DisconnectFn&& disconnect, 
							  AsyncRecvFn&& asyncRecv, IncrementCountFn&& incCount)
{
	for (;;)
	{
		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE)
			break;

		LanHeader lanHeader;
		session.recvBuf.Peek(&lanHeader, LAN_HEADER_SIZE);

		if (lanHeader.len > MAX_PAYLOAD_LEN)
		{
			disconnect();
			return;
		}

		if (session.recvBuf.GetUseSize() < LAN_HEADER_SIZE + lanHeader.len)
			break;

		session.recvBuf.MoveFront(LAN_HEADER_SIZE);

		PacketBuffer* contentsPacket = PacketBuffer::Alloc();
		char* tempBuffer = new char[lanHeader.len];
		session.recvBuf.Dequeue(tempBuffer, lanHeader.len);
		contentsPacket->PutData(tempBuffer, lanHeader.len);
		delete[] tempBuffer;

		onRecv(contentsPacket);
		incCount();
	}

	if (!session.disconnectFlag)
	{
		asyncRecv();
	}
}

// Recv 완료 처리 - NET 모드  
// - 암호화된 NET 헤더 기반 패킷을 복호화하고 OnRecv 콜백을 호출합니다.
template <typename OnRecvFn, typename DisconnectFn, typename AsyncRecvFn, typename IncrementCountFn>
inline void RecvCompletionNet(Session& session, BYTE protocolCode, BYTE privateKey,
							  OnRecvFn&& onRecv, DisconnectFn&& disconnect, 
							  AsyncRecvFn&& asyncRecv, IncrementCountFn&& incCount)
{
	for (;;)
	{
		if (session.recvBuf.GetUseSize() < NET_HEADER_SIZE)
			break;

		NetHeader netHeader;
		session.recvBuf.Peek(&netHeader, NET_HEADER_SIZE);

		if (netHeader.code != protocolCode)
		{
			disconnect();
			return;
		}

		if (netHeader.len > MAX_PAYLOAD_LEN)
		{
			disconnect();
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
			disconnect();
			return;
		}

		onRecv(decryptPacket);
		incCount();
	}

	if (!session.disconnectFlag)
	{
		asyncRecv();
	}
}

// IOCP Worker 함수 공통 로직
// - NetServer와 NetClient의 WorkerFunc 공통 부분을 추출한 템플릿 함수
template <typename SessionManagerT>
inline void IOCPWorkerLoop(HANDLE h_iocp, SessionManagerT& sessionManager)
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
			// Zero byte send 감지 (디버깅용 로그, 현재 비활성화)
			// if (&session->sendOverlapped == p_overlapped)
			// {
			//     LOG("IOCPWorker", LOG_LEVEL_FATAL, "Zero Byte Send !!");
			// }
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
				session->lastRecvTime = timeGetTime();
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

} // namespace IoCommon



