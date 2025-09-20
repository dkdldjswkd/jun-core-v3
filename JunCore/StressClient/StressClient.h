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

constexpr int SESSION_COUNT			= 10;
constexpr int MESSAGE_INTERVAL_MS	= 1;
constexpr int MESSAGE_MIN_SIZE		= 10;
constexpr int MESSAGE_MAX_SIZE		= 100;
constexpr int DISCONNECT_PROBABILITY_PER_THOUSAND = 1;
const std::string SERVER_IP			= "127.0.0.1";
constexpr WORD SERVER_PORT			= 7777;

struct SessionData;

class StressClient : public Client 
{
public:
    StressClient(std::shared_ptr<IOCPManager> manager);
    ~StressClient();

    void Start();
    void StopStressTest();
    bool IsRunning() const { return testRunning.load(); }

protected:
    void OnConnect(Session* session) override;
    void OnDisconnect(Session* session) override;
    void RegisterPacketHandlers() override;

private:
    void HandleEchoResponse(Session& session, const echo::EchoResponse& response);

private:
    std::vector<SessionData> session_data_vec;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> testRunning{false};
    
    void SessionWorker(int sessionIndex);
    std::string GenerateRandomMessage();
    SessionData* FindSessionData(Session* session);
    
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen;
    thread_local static std::uniform_int_distribution<> sizeDist;
    thread_local static std::uniform_int_distribution<> charDist;
};

struct SessionData
{
	Session* session = nullptr;
	std::thread workerThread;
	std::queue<std::string> sentMessages;
	int sessionIndex = 0;
	std::mt19937 randomGenerator{ std::random_device{}() };
	std::atomic<bool> disconnectRequested{false};  // 능동 끊기 플래그

	SessionData() = default;
	SessionData(Session* _session, int _sessionIndex)
		: session(_session), sessionIndex(_sessionIndex)
	{
	}
	// 복사 생성자와 대입 연산자 삭제 (atomic 멤버 때문)
	SessionData(const SessionData&) = delete;
	SessionData& operator=(const SessionData&) = delete;
	
	SessionData(SessionData&& other) noexcept
		: session(other.session)
		, workerThread(std::move(other.workerThread))
		, sentMessages(std::move(other.sentMessages))
		, sessionIndex(other.sessionIndex)
		, randomGenerator(std::move(other.randomGenerator))
		, disconnectRequested(other.disconnectRequested.load())
	{
		other.session = nullptr;
	}
	
	SessionData& operator=(SessionData&& other) noexcept
	{
		if (this != &other) {
			session = other.session;
			workerThread = std::move(other.workerThread);
			sentMessages = std::move(other.sentMessages);
			sessionIndex = other.sessionIndex;
			randomGenerator = std::move(other.randomGenerator);
			disconnectRequested.store(other.disconnectRequested.load());
			other.session = nullptr;
		}
		return *this;
	}
};