#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "echo_message.pb.h"
#include <vector>
#include <queue>
#include <thread>
#include <random>
#include <atomic>
#include <cassert>

constexpr int SESSION_COUNT = 10;
constexpr int MESSAGE_INTERVAL_MS = 10;
constexpr int MESSAGE_MIN_SIZE = 10;
constexpr int MESSAGE_MAX_SIZE = 100;
const std::string SERVER_IP = "127.0.0.1";
constexpr WORD SERVER_PORT = 7777;

struct SessionData {
    Session* session = nullptr;
    std::thread workerThread;
    std::queue<std::string> sentMessages;
    std::atomic<bool> running{false};
    int sessionIndex = 0;
    
    SessionData() = default;
    SessionData(SessionData&& other) noexcept 
        : session(other.session)
        , workerThread(std::move(other.workerThread))
        , sentMessages(std::move(other.sentMessages))
        , running(other.running.load())
        , sessionIndex(other.sessionIndex)
    {
        other.session = nullptr;
    }
    
    SessionData& operator=(SessionData&& other) noexcept {
        if (this != &other) {
            session = other.session;
            workerThread = std::move(other.workerThread);
            sentMessages = std::move(other.sentMessages);
            running = other.running.load();
            sessionIndex = other.sessionIndex;
            other.session = nullptr;
        }
        return *this;
    }
    
    SessionData(const SessionData&) = delete;
    SessionData& operator=(const SessionData&) = delete;
};

class StressClient : public Client 
{
public:
    StressClient(std::shared_ptr<IOCPManager> manager);
    ~StressClient();

    void StartStressTest();
    void StopStressTest();
    bool IsRunning() const { return testRunning.load(); }

protected:
    void OnConnect(Session* session) override;
    void OnDisconnect(Session* session) override;
    void RegisterPacketHandlers() override;

private:
    std::vector<SessionData> sessionList;
    std::atomic<bool> testRunning{false};
    
    void HandleEchoResponse(Session& session, const echo::EchoResponse& response);
    void SessionWorker(int sessionIndex);
    std::string GenerateRandomMessage();
    SessionData* FindSessionData(Session* session);
    
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen;
    thread_local static std::uniform_int_distribution<> sizeDist;
    thread_local static std::uniform_int_distribution<> charDist;
};