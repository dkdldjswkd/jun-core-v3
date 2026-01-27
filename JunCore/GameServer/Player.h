#pragma once
#include "../JunCore/logic/GameObject.h"
#include "../JunCore/network/User.h"
#include "../JunCore/protocol/game_messages.pb.h"
#include <string>

// Player는 GameObject 상속
// - GameObject는 이미 JobObject를 상속하므로 JobObject 기능 모두 사용 가능
// - OnUpdate, OnFixedUpdate 등 게임 로직
// - PostJob으로 패킷 핸들러 등을 Job으로 처리
class Player : public GameObject
{
public:
	Player(GameScene* scene, User* owner, uint32_t player_id);
	~Player();

protected:
	// ──────────────────────────────────────────────────────
	// GameObject 인터페이스 구현
	// ──────────────────────────────────────────────────────
	void OnUpdate() override;
	void OnFixedUpdate() override;
	void OnEnter() override;
	void OnExit() override;

public:
	// ──────────────────────────────────────────────────────
	// Player 전용 메서드
	// ──────────────────────────────────────────────────────

	// 목표 위치 설정 Job 등록
	void PostSetDestPosJob(const game::Pos& dest_pos);

	// 패킷 전송 (User를 통해)
	template<typename T>
	void SendPacket(const T& packet)
	{
		if (owner_)
		{
			owner_->SendPacket(packet);
		}
	}

	// Scene 내 모든 Player에게 브로드캐스트
	template<typename T>
	void BroadcastToScene(const T& packet)
	{
		if (!m_pScene)
			return;

		const auto& objects = m_pScene->GetObjects();
		for (auto* obj : objects)
		{
			Player* player = dynamic_cast<Player*>(obj);
			if (player && player->owner_)
			{
				player->owner_->SendPacket(packet);
			}
		}
	}

	// Scene 내 다른 Player들에게 브로드캐스트 (자신 제외)
	template<typename T>
	void BroadcastToOthers(const T& packet)
	{
		if (!m_pScene)
			return;

		const auto& objects = m_pScene->GetObjects();
		for (auto* obj : objects)
		{
			Player* player = dynamic_cast<Player*>(obj);
			if (player && player != this && player->owner_)
			{
				player->owner_->SendPacket(packet);
			}
		}
	}

	// Getter
	User* GetOwner() const { return owner_; }
	uint32_t GetPlayerId() const { return player_id_; }

	const game::Pos& GetCurrentPos() const { return currentPos_; }
	const game::Pos& GetDestPos() const { return destPos_; }

private:
	// ──────────────────────────────────────────────────────
	// 내부 Job 핸들러들 (LogicThread에서 실행됨)
	// ──────────────────────────────────────────────────────
	void HandleSetDestPos(const game::Pos& dest_pos);

	// ──────────────────────────────────────────────────────
	// 이동 관련 헬퍼
	// ──────────────────────────────────────────────────────
	bool HasReachedDestination() const;
	void MoveTowardsDestination();

private:
	User* owner_;               // 소유 네트워크 세션
	uint32_t player_id_;        // 플레이어 ID

	// 플레이어 상태
	game::Pos currentPos_;      // 현재 위치
	game::Pos destPos_;         // 목표 위치
	float moveSpeed_{0.1f};     // FixedUpdate당 이동 거리 (50Hz 기준, 초당 5m)
};
