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

void StressClient::StartStressTest()
{
	if (testRunning.exchange(true))
	{
		LOG_WARN("[StressTest] Already running!");
		return;
	}

	LOG_INFO("[StressTest] Starting stress test with %d sessions...", SESSION_COUNT);

	for (int _session_index = 0; _session_index < SESSION_COUNT; _session_index++)
	{
        if (Session* session = Connect())
        {
			session_data_vec.emplace_back(session, _session_index, true);
			session_data_vec.back().workerThread = std::thread(&StressClient::SessionWorker, this, _session_index);

			LOG_INFO("[StressTest] Session %d connected: 0x%llX", _session_index, (uintptr_t)session);
        }
        else
        {
            LOG_ERROR("[StressTest] Failed to connect session %d", _session_index);
            assert(false);
            continue;
        }
    }
    
    LOG_INFO("[StressTest] All %d sessions connected successfully!", SESSION_COUNT);
}

void StressClient::StopStressTest()
{
    if (!testRunning.exchange(false)) 
    {
        return;
    }
    
    LOG_INFO("[StressTest] Stopping stress test...");
    
    for (auto& sessionData : session_data_vec) 
    {
        sessionData.running = false;
        if (sessionData.workerThread.joinable()) 
        {
            sessionData.workerThread.join();
        }
        
        if (sessionData.session) 
        {
            Disconnect(sessionData.session);
        }
    }
    
    session_data_vec.clear();
    LOG_INFO("[StressTest] Stress test stopped.");
}

void StressClient::SessionWorker(int sessionIndex)
{
    LOG_ASSERT_RETURN_VOID(sessionIndex <= session_data_vec.size(), "[StressTest] Invalid session index: %d", sessionIndex);

	SessionData& sessionData = session_data_vec[sessionIndex];
	LOG_INFO("[StressTest][Session %d] Worker thread started", sessionIndex);

	while (sessionData.running.load())
	{
		if (!sessionData.session)
		{
			break;
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

	LOG_INFO("[StressTest][Session %d] Worker thread ended", sessionIndex);
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
	// 스트레스 클라이언트는 disconnect 되면 안됨
	LOG_ERROR("[StressTest] Session disconnected: 0x%llX", (uintptr_t)session);
	assert(false);
}