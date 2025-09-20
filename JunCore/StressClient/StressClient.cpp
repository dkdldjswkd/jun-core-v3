#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "StressClient.h"
#include <iostream>
#include <chrono>

thread_local std::random_device StressClient::rd;
thread_local std::mt19937 StressClient::gen(rd());
thread_local std::uniform_int_distribution<> StressClient::sizeDist(MESSAGE_MIN_SIZE, MESSAGE_MAX_SIZE);
thread_local std::uniform_int_distribution<> StressClient::charDist('A', 'Z');

StressClient::StressClient(std::shared_ptr<IOCPManager> manager) 
    : Client(manager, SERVER_IP.c_str(), SERVER_PORT)
{
    session_data_vec.reserve(SESSION_COUNT);
}

StressClient::~StressClient()
{
    StopStressTest();
}

void StressClient::RegisterPacketHandlers()
{
    RegisterPacketHandler<echo::EchoResponse>([this](Session& session, const echo::EchoResponse& response) 
    {
        HandleEchoResponse(session, response);
    });
}

void StressClient::Start()
{
	if (testRunning.exchange(true))
	{
		LOG_WARN("[StressTest] Already running!");
		return;
	}

	LOG_INFO("[StressTest] Starting stress test with %d sessions...", SESSION_COUNT);

	session_data_vec.resize(SESSION_COUNT);
	workerThreads.reserve(SESSION_COUNT);

	// 워커 스레드들만 실행 (세션은 각 워커에서 생성)
	for (int i = 0; i < SESSION_COUNT; i++)
	{
		workerThreads.emplace_back(&StressClient::SessionWorker, this, i);
	}
	
	LOG_INFO("[StressTest] All %d worker threads started!", SESSION_COUNT);
}

void StressClient::StopStressTest()
{
    if (!testRunning.exchange(false)) 
    {
        return;
    }
    
    LOG_INFO("[StressTest] Stopping stress test...");
    
    // 모든 워커 스레드 종료 대기
    for (auto& t : workerThreads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    
    // 리소스 정리
    for (auto& sessionData : session_data_vec) 
    {
        if (sessionData.session) 
        {
            DisconnectSession(sessionData.session);
        }
    }
    
    workerThreads.clear();
    session_data_vec.clear();
    LOG_INFO("[StressTest] Stress test stopped.");
}

void StressClient::HandleEchoResponse(Session& session, const echo::EchoResponse& response)
{
	SessionData* sessionData = FindSessionData(&session);
	LOG_ASSERT_RETURN_VOID(sessionData, "[StressTest] Cannot find session data for 0x%llX", (uintptr_t)&session);

	// sentMessages 비어있다면 에러
	LOG_ASSERT_RETURN_VOID(!sessionData->sentMessages.empty(), "[StressTest][Session %d] Received response but no sent messages!", sessionData->sessionIndex);

	std::string expectedMessage = sessionData->sentMessages.front();
	sessionData->sentMessages.pop();

	// Echo 받은 메시지가 내가 보낸 메시지와 다르면 에러
	LOG_ASSERT_RETURN_VOID(response.message() == expectedMessage,
		"[StressTest][Session %d] Message mismatch! Expected: '%s', Got: '%s'",
		sessionData->sessionIndex, expectedMessage.c_str(), response.message().c_str());

	// 디버깅 로그
	// LOG_DEBUG("[StressTest][Session %d] Message verified: '%s'", sessionData->sessionIndex, response.message().c_str());
}

void StressClient::SessionWorker(int sessionIndex)
{
    LOG_ASSERT_RETURN_VOID(sessionIndex < SESSION_COUNT, "[StressTest] Invalid session index: %d", sessionIndex);

	session_data_vec[sessionIndex] = std::move(SessionData(nullptr, sessionIndex));
	SessionData& sessionData = session_data_vec[sessionIndex];

	LOG_INFO("[StressTest][Session %d] Worker thread started", sessionIndex);

	while (testRunning.load())
	{
		if (sessionData.session == nullptr)
		{
            if (sessionData.session = Connect())
            {
				sessionData.disconnectRequested = false;
				while (!sessionData.sentMessages.empty())
				{
					sessionData.sentMessages.pop();
				}
            }
            else
            {
			    LOG_ASSERT(false, "[StressTest][Worker %d] Failed to connect", sessionIndex);
            }
		}

		// Session disconnect 요청했다면,
		if (sessionData.disconnectRequested.load()) 
		{
			// Session 정리 대기
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		// 랜덤 disconnect
		std::uniform_int_distribution<> dist(1, 1000);
		if (dist(sessionData.randomGenerator) <= DISCONNECT_PROBABILITY_PER_THOUSAND)
		{
			sessionData.disconnectRequested = true;
			DisconnectSession(sessionData.session);
			// session 포인터는 OnDisconnect에서 무효화
			continue;
		}

		std::string message = GenerateRandomMessage();
		sessionData.sentMessages.push(message);

		echo::EchoRequest request;
		request.set_message(message);

		// 패킷 송신, 실패 시 어설트
		LOG_ASSERT(sessionData.session->SendPacket(request), "[StressTest][Session %d] Failed to send message", sessionIndex);

		// 스트레스 테스트를 위한 최소 간격
		if (0 < MESSAGE_INTERVAL_MS)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(MESSAGE_INTERVAL_MS));
		}
	}

	// 정리
	if (sessionData.session)
	{
		DisconnectSession(sessionData.session);
	}

	LOG_INFO("[StressTest][Session %d] Worker thread ended", sessionIndex);
}

std::string StressClient::GenerateRandomMessage()
{
    int length = sizeDist(gen);
    std::string message;
    message.reserve(length);
    
    for (int i = 0; i < length; i++) 
    {
        message += static_cast<char>(charDist(gen));
    }
    
    return message;
}

SessionData* StressClient::FindSessionData(Session* session)
{
    for (auto& sessionData : session_data_vec) 
    {
        if (sessionData.session == session) 
        {
            return &sessionData;
        }
    }
    return nullptr;
}

void StressClient::OnConnect(Session* session)
{
	LOG_INFO("[StressTest] Session connected: 0x%llX", (uintptr_t)session);
}

void StressClient::OnDisconnect(Session* session)
{
	SessionData* sessionData = FindSessionData(session);
	if (sessionData)
	{
		if (sessionData->disconnectRequested)
		{
			// 능동적 disconnect의 경우
			LOG_INFO("[StressTest] Expected disconnect for session %d: 0x%llX", sessionData->sessionIndex, (uintptr_t)session);
		}
		else
		{
			// 서버에 의해 끊킨것이므로 에러
			LOG_ERROR("[StressTest] Unexpected server disconnect for session %d: 0x%llX", sessionData->sessionIndex, (uintptr_t)session);
		}
		
		// Session* 무효화
		sessionData->session = nullptr;
	}
	else
	{
		// 에러
		LOG_WARN("[StressTest] Disconnect for unknown session: 0x%llX", (uintptr_t)session);
	}
}

