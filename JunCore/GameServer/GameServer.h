#pragma once
#include "../JunCore/network/Server.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "../JunCore/protocol/game_messages.pb.h"
#include <atomic>
#include <unordered_map>
#include <memory>

// Forward declarations
class Player;
class GameScene;  // Forward declaration 유지 (헤더에서는 포인터만 사용)

class GameServer : public Server
{
public:
	GameServer(std::shared_ptr<IOCPManager> manager, int logic_thread_count);
	~GameServer();

public:
	// 세션 모니터링 함수
	uint32_t GetCurrentSessions() const { return currentSessions_.load(); }
	uint32_t GetTotalConnected() const { return totalConnected_.load(); }
	uint32_t GetTotalDisconnected() const { return totalDisconnected_.load(); }

private:
	// ──────────────────────────────────────────────────────
	// 패킷 핸들러들
	// ──────────────────────────────────────────────────────
	void HandleLoginRequest(User& user, const game::CG_LOGIN_REQ& request);
	void HandleMoveRequest(User& user, const game::CG_MOVE_REQ& request);

	// Scene 선택 로직 (TODO: 향후 여러 Scene 지원 시 확장)
	GameScene* ChooseSceneForNewPlayer();

	// ──────────────────────────────────────────────────────
	// Server 가상 함수 오버라이드
	// ──────────────────────────────────────────────────────
	void OnSessionConnect(User* user) override;
	void OnUserDisconnect(User* user) override;
	void OnServerStart() override;
	void OnServerStop() override;
	void RegisterPacketHandlers() override;

private:
	// ──────────────────────────────────────────────────────
	// 게임 로직 관리
	// ──────────────────────────────────────────────────────
	std::unique_ptr<GameScene> scene_;  // 메인 게임 씬 (TODO: GameScene 구현)

	// ──────────────────────────────────────────────────────
	// 세션 모니터링
	// ──────────────────────────────────────────────────────
	std::atomic<uint32_t> currentSessions_{0};
	std::atomic<uint32_t> totalConnected_{0};
	std::atomic<uint32_t> totalDisconnected_{0};
};
