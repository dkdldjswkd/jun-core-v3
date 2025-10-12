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
    RegisterPacketHandler<echo::EchoResponse>([this](User& user, const echo::EchoResponse& response) 
    {
        HandleEchoResponse(user, response);
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
        if (sessionData.user) 
        {
			sessionData.user->Disconnect();
        }
    }
    
    workerThreads.clear();
    session_data_vec.clear();
    LOG_INFO("[StressTest] Stress test stopped.");
}

void StressClient::HandleEchoResponse(User& user, const echo::EchoResponse& response)
{
	SessionData* sessionData = FindSessionData(&user);
	LOG_ASSERT_RETURN_VOID(sessionData, "[StressTest] Cannot find session data for 0x%llX", (uintptr_t)&user);

	// sentMessages에서 메시지 dequeue 시도
	std::string expectedMessage;
	if (!sessionData->sentMessages.Dequeue(&expectedMessage))
	{
		LOG_ASSERT("[StressTest][Session %d] Received response but no sent messages!", sessionData->sessionIndex);
		return;
	}

	std::string responseMessage = response.message();
	
	// Echo 받은 메시지가 내가 보낸 메시지와 다르면 에러
	LOG_ASSERT_RETURN_VOID(expectedMessage == responseMessage,
						   "[StressTest][Session %d] Message mismatch! Expected: '%s', Got: '%s'",
						   sessionData->sessionIndex, expectedMessage.c_str(), responseMessage.c_str());
}

void StressClient::SessionWorker(int sessionIndex)
{
    LOG_ASSERT_RETURN_VOID(sessionIndex < SESSION_COUNT, "[StressTest] Invalid session index: %d", sessionIndex);

	session_data_vec[sessionIndex] = std::move(SessionData(nullptr, sessionIndex));
	SessionData& sessionData = session_data_vec[sessionIndex];

	LOG_INFO("[StressTest][Session %d] Worker thread started", sessionIndex);

	while (testRunning.load())
	{
		if (sessionData.user == nullptr)
		{
            if (sessionData.user = Connect())
            {
				sessionData.disconnectRequested = false;
				sessionData.totalSendCount = 0;  // 새 연결 시 카운터 초기화
				std::string msg;
				while (sessionData.sentMessages.Dequeue(&msg)); // Queue 비우기
            }
            else
            {
			    LOG_ASSERT("[StressTest][Worker %d] Failed to connect", sessionIndex);
				return;
            }
		}

		// 정리 대기 중인 세션이라면,
		if (sessionData.disconnectRequested)
		{
			// 정리 대기
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		// 랜덤 disconnect
		std::uniform_int_distribution<> dist(1, 1000);
		if (dist(sessionData.randomGenerator) <= DISCONNECT_PROBABILITY_PER_THOUSAND)
		{
			sessionData.disconnectRequested = true;
			sessionData.user->Disconnect();
			// user 포인터는 OnDisconnect에서 무효화
			continue;
		}

		std::string message = GenerateRandomMessage();
		sessionData.sentMessages.Enqueue(message);

		echo::EchoRequest request;
		request.set_message(message);

		// 패킷 송신, 실패 시 어설트 
		// 실패 case 1. 서버에서 세션을 끊음 -> 스트레스 클라이언트를 끊을 이유가 없다. 서버 이슈
		// 실패 case 2. 스트레스 클라이언트에서 Disconnect 한것 -> Disconnect 세션에 대해서는 패킷 송신하지 않는다. 클라이언트 이슈.
		if (!sessionData.user->SendPacket(request))
		{
			LOG_ASSERT("[StressTest][Session %d] Failed to send message after %d successful sends", sessionIndex, sessionData.totalSendCount);
			sessionData.user->Disconnect();
			return;
		}
		
		// 송신 성공 시 카운터 증가
		sessionData.totalSendCount++;
		
		// 스트레스 테스트를 위한 최소 간격
		if (0 < MESSAGE_INTERVAL_MS)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(MESSAGE_INTERVAL_MS));
		}
	}

	// 정리
	if (sessionData.user)
	{
		sessionData.user->Disconnect();
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

SessionData* StressClient::FindSessionData(User* user)
{
    for (auto& sessionData : session_data_vec) 
    {
        if (sessionData.user == user) 
        {
            return &sessionData;
        }
    }
    return nullptr;
}


void StressClient::OnUserDisconnect(User* user)
{
	SessionData* sessionData = FindSessionData(user);
	if (sessionData)
	{
		if (sessionData->disconnectRequested)
		{
			// 능동적 disconnect의 경우
			LOG_INFO("[StressTest] Expected disconnect for session %d: 0x%llX", sessionData->sessionIndex, (uintptr_t)user);
		}
		else
		{
			// 서버에 의해 끊킨것이므로 에러
			LOG_ASSERT("[StressTest] Unexpected server disconnect for session %d: 0x%llX", sessionData->sessionIndex, (uintptr_t)user);
		}
		
		// User* 무효화
		sessionData->user = nullptr;
	}
	else
	{
		// 에러
		LOG_ASSERT("[StressTest] Disconnect for unknown user: 0x%llX", (uintptr_t)user);
	}
}

