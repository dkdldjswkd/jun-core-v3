#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "../JunCommon/container/LFQueue.h"
#include "echo_message.pb.h"
#include <vector>
#include <thread>
#include <random>
#include <atomic>
#include <cassert>

constexpr int SESSION_COUNT			= 200;
constexpr int MESSAGE_INTERVAL_MS	= 1; // 최소 1 이상 권장, 0은 SendQ 터지는 현상 발생함
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
    void OnUserDisconnect(User* user) override;
    void RegisterPacketHandlers() override;

private:
    void HandleEchoResponse(User& user, const echo::EchoResponse& response);

private:
    std::vector<SessionData> session_data_vec;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> testRunning{false};
    
    void SessionWorker(int sessionIndex);
    std::string GenerateRandomMessage();
    SessionData* FindSessionData(User* user);
    
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen;
    thread_local static std::uniform_int_distribution<> sizeDist;
    thread_local static std::uniform_int_distribution<> charDist;
};

struct SessionData
{
	User* user = nullptr;
	std::thread workerThread;
	LFQueue<std::string> sentMessages;
	int sessionIndex = 0;
	std::mt19937 randomGenerator{ std::random_device{}() };
	std::atomic<bool> disconnectRequested{false};  // 능동 끊기 플래그
	int totalSendCount = 0;  // 누적 send 회수

	SessionData() = default;
	SessionData(User* _user, int _sessionIndex)
		: user(_user), sessionIndex(_sessionIndex)
	{
	}
	// 복사 생성자와 대입 연산자 삭제 (atomic 멤버 때문)
	SessionData(const SessionData&) = delete;
	SessionData& operator=(const SessionData&) = delete;
	
	SessionData(SessionData&& other) noexcept
		: user(other.user)
		, workerThread(std::move(other.workerThread))
		, sessionIndex(other.sessionIndex)
		, randomGenerator(std::move(other.randomGenerator))
		, disconnectRequested(other.disconnectRequested.load())
		, totalSendCount(other.totalSendCount)
	{
		other.user = nullptr;
		other.totalSendCount = 0;
		// LFQueue는 이동 생성자가 없으므로 빈 큐로 초기화됨
	}
	
	SessionData& operator=(SessionData&& other) noexcept
	{
		if (this != &other) {
			user = other.user;
			workerThread = std::move(other.workerThread);
			sessionIndex = other.sessionIndex;
			randomGenerator = std::move(other.randomGenerator);
			disconnectRequested.store(other.disconnectRequested.load());
			totalSendCount = other.totalSendCount;
			other.user = nullptr;
			other.totalSendCount = 0;
			// LFQueue는 이동 대입 연산자가 없으므로 기존 큐 유지
		}
		return *this;
	}
};