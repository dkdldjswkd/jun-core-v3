#include "IOCPManager.h"
#include "NetBase.h"
#include "Server.h"
#include "Client.h"
#include "../protocol/UnifiedPacketHeader.h"

//------------------------------
// IOCPManager 구현 - 패킷 조립 로직
//------------------------------

void IOCPManager::RunWorkerThread()
{
    thread_local TimeWindowCounter<uint64_t> tlsRecvCounter;
    thread_local TimeWindowCounter<uint64_t> tlsSendCounter;
    
    static std::atomic<int> nextWorkerIndex{0};
    const int workerIndex = nextWorkerIndex.fetch_add(1);
    
    recvCounters[workerIndex] = &tlsRecvCounter;
    sendCounters[workerIndex] = &tlsSendCounter;
    
    for (;;) 
    {
        DWORD ioSize                = 0;
        ULONG_PTR completionKey     = 0; // 사용하지 않음
        OverlappedEx* p_overlapped  = nullptr;

		BOOL retGQCS = GetQueuedCompletionStatus(iocpHandle, &ioSize, &completionKey, (LPOVERLAPPED*)&p_overlapped, INFINITE);

        // IOCP 종료 메시지
        if (ioSize == 0 && p_overlapped == nullptr)
        {
            // 다른 Worker에게도 종료를 알린다.
			PostQueuedCompletionStatus(iocpHandle, 0, 0, 0);
            break;
        }

		if (ioSize == 0 && p_overlapped->operation_ != IOOperation::IO_ACCEPT && p_overlapped->operation_ != IOOperation::IO_CONNECT)
        {
            goto DecrementIOCount;
        }

		if (PQCS::none < static_cast<PQCS>(reinterpret_cast<uintptr_t>(p_overlapped)) && static_cast<PQCS>(reinterpret_cast<uintptr_t>(p_overlapped)) < PQCS::max)
		{
			goto DecrementIOCount;
		}

		// 수신 완료 처리
        switch (p_overlapped->operation_)
        {
		case IOOperation::IO_RECV:
		{
			if (enableMonitoring)
			{
				tlsRecvCounter.record(ioSize);
			}
			HandleRecvComplete(p_overlapped->session_.get(), ioSize);
		} break;

		case IOOperation::IO_SEND:
		{
			if (enableMonitoring)
			{
				tlsSendCounter.record(ioSize);
			}
			HandleSendComplete(p_overlapped->session_.get());
		} break;

		case IOOperation::IO_ACCEPT:
		{
			HandleAcceptComplete(p_overlapped->session_.get(), ioSize);
		} break;

		case IOOperation::IO_CONNECT:
		{
			HandleConnectComplete(p_overlapped->session_.get(), ioSize);
		} break;

		default:
			LOG_ERROR("invalid io operation : %d", p_overlapped->operation_);
			break;
        }

	DecrementIOCount:
		delete p_overlapped;
	}
}

void IOCPManager::HandleRecvComplete(Session* session, DWORD ioSize)
{
	int loopCount = 0;
	const int MAX_LOOP_COUNT = 1000;

    // 수신 버퍼 업데이트
    session->recv_buf_.MoveRear(ioSize);
    session->last_recv_time_ = static_cast<DWORD>(GetTickCount64());

	for (;;)
	{
		// 무한루프 방지
		LOG_ERROR_RETURN_VOID(++loopCount <= MAX_LOOP_COUNT, "IOCPManager: MAX_LOOP_COUNT reached");

		const int _recv_byte = session->recv_buf_.GetUseSize();

		// 최소 데이터 수신 여부 체크 (UnifiedPacketHeader 크기)
		if (_recv_byte <= UNIFIED_HEADER_SIZE)
		{
			break; 
		}

		uint32_t _packet_len;
		session->recv_buf_.Peek(&_packet_len, sizeof(uint32_t));

		// 패킷 크기 유효성 검사
		LOG_ERROR_RETURN_VOID(IsValidPacketSize(_packet_len), "Invalid packet length: %d", _packet_len);

		// 충분한 패킷 데이터가 도착했는지 확인
        if (static_cast<uint32_t>(_recv_byte) < _packet_len)
        {
            break;
        }

        // 패킷 추출
		std::vector<char> packet(_packet_len);
        session->recv_buf_.Dequeue(&packet[0], _packet_len);

        const UnifiedPacketHeader* header = reinterpret_cast<const UnifiedPacketHeader*>(&packet[0]);

        const uint32_t _packet_id = header->packet_id;
        LOG_DEBUG("Received packet: id=%u, size=%u", _packet_id, _packet_len);

        // Payload 추출
        std::vector<char> payload(packet.begin() + UNIFIED_HEADER_SIZE, packet.end());

        if (NetBase* engine = session->GetEngine())
        {
            engine->OnPacketReceived(session, header->packet_id, payload);
        }
        else 
        {
            LOG_ERROR("Session has no engine assigned");
            return;
        }
	}

	if (!session->pending_disconnect_)
	{
		session->RecvAsync();
	}
}

void IOCPManager::HandleSendComplete(Session* session)
{
#ifdef _DEBUG
    auto handling_count_ = session->send_complete_handling_count_.fetch_add(1);
    if (0 < handling_count_)
    {
        CRASH();
    }
#endif

    // 송신 완료 후처리
	{
		for (int i = 0; i < session->send_packet_count_; i++)
		{
			delete session->send_packet_arr_[i];
			session->send_packet_arr_[i] = nullptr;
		}
		session->send_packet_count_ = 0;
		session->send_flag_.store(false);
	}

#ifdef _DEBUG
    session->send_complete_handling_count_--;
#endif

    // 추가 송신 필요 여부 확인 및 추가 송신
    if (!session->pending_disconnect_ && session->send_q_.GetUseCount() > 0)
    {
        session->SendAsync();
    }
}

//------------------------------
// 네트워크 통계 조회 함수들
//------------------------------
double IOCPManager::GetRecvBytesPerSecond(int seconds) const
{
    double total = 0.0;
    for (auto* counter : recvCounters) 
    {
        if (counter != nullptr) 
        {
            total += counter->get_average(seconds);
        }
        else
        {
			LOG_ERROR("recvCounters contains a null pointer");
        }
    }
    return total;
}

double IOCPManager::GetSendBytesPerSecond(int seconds) const
{
    double total = 0.0;
    for (auto* counter : sendCounters) 
    {
        if (counter != nullptr) 
        {
            total += counter->get_average(seconds);
        }
		else
		{
			LOG_ERROR("sendCounters contains a null pointer");
		}
    }
    return total;
}

void IOCPManager::HandleAcceptComplete(Session* session, DWORD ioSize)
{
    SOCKET acceptSocket;
    class NetBase* engine;
    class Server* server = nullptr;
    SOCKET listenSocket;
    SOCKADDR_IN clientAddr;
    int clientAddrLen;
    User* user = nullptr;
    
    // Session에서 필요한 정보 추출
    acceptSocket = session->sock_;
    engine = session->GetEngine();
    server = dynamic_cast<class Server*>(engine);
    
    if (!server)
    {
        LOG_ERROR("Session engine is not a Server instance");
        closesocket(acceptSocket);
        goto PostNewAccept;
    }
    
    // AcceptEx 완료 후 필수 setsockopt 호출
    listenSocket = server->listenSocket;
    if (setsockopt(acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, 
                   (char*)&listenSocket, sizeof(listenSocket)) == SOCKET_ERROR)
    {
        LOG_ERROR("setsockopt SO_UPDATE_ACCEPT_CONTEXT failed: %d", WSAGetLastError());
        closesocket(acceptSocket);
        goto PostNewAccept;
    }
    
    // 클라이언트 주소 정보 추출
    clientAddrLen = sizeof(clientAddr);
    if (getpeername(acceptSocket, (SOCKADDR*)&clientAddr, &clientAddrLen) == SOCKET_ERROR)
    {
        LOG_ERROR("getpeername failed: %d", WSAGetLastError());
        closesocket(acceptSocket);
        goto PostNewAccept;
    }
    
    // Session 완전 설정
    user = new User(session->shared_from_this());
    session->Set(acceptSocket, clientAddr.sin_addr, ntohs(clientAddr.sin_port), server, iocpHandle, user);
    server->OnSessionConnect(user);
    session->RecvAsync();

PostNewAccept:
    if (server && !server->PostAcceptEx())
    {
        LOG_ERROR("Failed to post new AcceptEx - server may stop accepting connections");
    }
}

void IOCPManager::HandleConnectComplete(Session* session, DWORD ioSize)
{
    SOCKET connectSocket;
    class NetBase* engine;
    class Client* client = nullptr;
    User* user = nullptr;
    bool connectSuccess = true;
    
    // Session에서 필요한 정보 추출
    connectSocket = session->sock_;
    engine = session->GetEngine();
    client = dynamic_cast<class Client*>(engine);
    
    if (!client)
    {
        LOG_ERROR("Session engine is not a Client instance");
        closesocket(connectSocket);
        return;
    }
    
    // ConnectEx 완료 후 필수 setsockopt 호출
    if (setsockopt(connectSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 
                   NULL, 0) == SOCKET_ERROR)
    {
        LOG_ERROR("setsockopt SO_UPDATE_CONNECT_CONTEXT failed: %d", WSAGetLastError());
        connectSuccess = false;
    }
    
    if (connectSuccess)
    {
        // 연결 성공 - User 생성 및 세션 완전 설정
        user = new User(session->shared_from_this());

        // 클라이언트의 로컬 주소 정보 추출
        SOCKADDR_IN localAddr;
        int localAddrLen = sizeof(localAddr);
        if (getsockname(connectSocket, (SOCKADDR*)&localAddr, &localAddrLen) == SOCKET_ERROR)
        {
            LOG_WARN("getsockname failed: %d", WSAGetLastError());
            ZeroMemory(&localAddr, sizeof(localAddr));
        }

        session->Set(connectSocket, localAddr.sin_addr, ntohs(localAddr.sin_port), client, iocpHandle, user);
        session->RecvAsync();

        LOG_INFO("Connection established successfully");
        client->OnConnectComplete(user, true);
    }
    else
    {
        // 연결 실패 - 재연결 트리거
        LOG_ERROR("Connection failed, triggering reconnect");
        closesocket(connectSocket);
        client->TriggerReconnect();
        client->OnConnectComplete(nullptr, false);
    }
}