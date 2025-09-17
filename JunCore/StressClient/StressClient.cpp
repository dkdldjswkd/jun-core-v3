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
    sessionList.reserve(SESSION_COUNT);
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
    if (testRunning.exchange(true)) {
        LOG_INFO("[StressTest] Already running!");
        return;
    }
    
    LOG_INFO("[StressTest] Starting stress test with %d sessions...", SESSION_COUNT);
    
    for (int i = 0; i < SESSION_COUNT; i++) {
        Session* session = Connect();
        if (!session) {
            LOG_ERROR("[StressTest] Failed to connect session %d", i);
            continue;
        }
        
        SessionData sessionData;
        sessionData.session = session;
        sessionData.sessionIndex = i;
        sessionData.running = true;
        
        // 먼저 vector에 추가 (Race Condition 방지)
        sessionList.push_back(std::move(sessionData));
        
        // 그 다음에 세션별 워커 스레드 시작
        sessionList.back().workerThread = std::thread(&StressClient::SessionWorker, this, i);
        
        LOG_INFO("[StressTest] Session %d connected: 0x%llX", i, (uintptr_t)session);
    }
    
    LOG_INFO("[StressTest] All %d sessions connected successfully!", SESSION_COUNT);
}

void StressClient::StopStressTest()
{
    if (!testRunning.exchange(false)) {
        return;
    }
    
    LOG_INFO("[StressTest] Stopping stress test...");
    
    for (auto& sessionData : sessionList) {
        sessionData.running = false;
        if (sessionData.workerThread.joinable()) {
            sessionData.workerThread.join();
        }
        
        if (sessionData.session) {
            Disconnect(sessionData.session);
        }
    }
    
    sessionList.clear();
    LOG_INFO("[StressTest] Stress test stopped.");
}

void StressClient::OnConnect(Session* session)
{
    LOG_INFO("[StressTest] Session connected: 0x%llX", (uintptr_t)session);
}

void StressClient::OnDisconnect(Session* session)
{
    LOG_INFO("[StressTest] Session disconnected: 0x%llX", (uintptr_t)session);
}

void StressClient::HandleEchoResponse(Session& session, const echo::EchoResponse& response)
{
    SessionData* sessionData = FindSessionData(&session);
    if (!sessionData) {
        LOG_ERROR("[StressTest] Cannot find session data for 0x%llX", (uintptr_t)&session);
        return;
    }
    
    if (sessionData->sentMessages.empty()) {
        LOG_ERROR("[StressTest][Session %d] Received response but no sent messages!", sessionData->sessionIndex);
        return;
    }
    
    std::string expectedMessage = sessionData->sentMessages.front();
    sessionData->sentMessages.pop();
    
    if (response.message() != expectedMessage) {
        LOG_ERROR("[StressTest][Session %d] Message mismatch! Expected: '%s', Got: '%s'", 
                  sessionData->sessionIndex, expectedMessage.c_str(), response.message().c_str());
        return;
    }
    
    LOG_DEBUG("[StressTest][Session %d] Message verified: '%s'", sessionData->sessionIndex, response.message().c_str());
}

void StressClient::SessionWorker(int sessionIndex)
{
    if (sessionIndex >= sessionList.size()) {
        LOG_ERROR("[StressTest] Invalid session index: %d", sessionIndex);
        return;
    }
    
    SessionData& sessionData = sessionList[sessionIndex];
    LOG_INFO("[StressTest][Session %d] Worker thread started", sessionIndex);
    
    while (sessionData.running.load()) {
        if (!sessionData.session) {
            break;
        }
        
        // 연속적으로 메시지 생성 및 전송 (받던 말던 계속 던짐)
        std::string message = GenerateRandomMessage();
        
        echo::EchoRequest request;
        request.set_message(message);
        
        if (sessionData.session->SendPacket(request)) {
            // 성공적으로 보낸 메시지 순서 추적 (순서 검증용)
            sessionData.sentMessages.push(message);
            LOG_DEBUG("[StressTest][Session %d] Sent message: '%s'", sessionIndex, message.c_str());
        } else {
            LOG_ERROR("[StressTest][Session %d] Failed to send message", sessionIndex);
        }
        
        // 스트레스 테스트를 위한 최소 간격
        std::this_thread::sleep_for(std::chrono::milliseconds(MESSAGE_INTERVAL_MS));
    }
    
    LOG_INFO("[StressTest][Session %d] Worker thread ended", sessionIndex);
}

std::string StressClient::GenerateRandomMessage()
{
    int length = sizeDist(gen);
    std::string message;
    message.reserve(length);
    
    for (int i = 0; i < length; i++) {
        message += static_cast<char>(charDist(gen));
    }
    
    return message;
}

SessionData* StressClient::FindSessionData(Session* session)
{
    for (auto& sessionData : sessionList) {
        if (sessionData.session == session) {
            return &sessionData;
        }
    }
    return nullptr;
}