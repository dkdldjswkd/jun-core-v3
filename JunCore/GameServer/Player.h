#pragma once
#include "../JunCore/logic/GameObject.h"
#include "../JunCore/logic/GameScene.h"
#include "../JunCore/core/Event.h"
#include "MoveComponent.h"
#include "AttackComponent.h"
#include "../JunCore/network/User.h"
#include "protocol/game_messages.pb.h"
#include <string>

// Player는 GameObject 상속
// - GameObject는 이미 Entity와 JobObject를 상속하므로 컴포넌트 및 Job 기능 사용 가능
// - OnUpdate, OnFixedUpdate 등 게임 로직
// - PostJob으로 패킷 핸들러 등을 Job으로 처리
// - MoveComponent를 통해 이동 로직 분리
class Player : public GameObject
{
public:
	Player(GameScene* scene, User* owner, uint32_t player_id, float x, float y, float z);
	~Player();

protected:
	// ──────────────────────────────────────────────────────
	// GameObject 인터페이스 구현
	// ──────────────────────────────────────────────────────
	void OnUpdate() override;
	void OnFixedUpdate() override;
	void OnEnter() override;
	void OnExit() override;

	// AOI 이벤트: 내 시야에 다른 오브젝트가 들어옴/나감
	void OnAppear(std::vector<GameObject*>& others) override;
	void OnDisappear(std::vector<GameObject*>& others) override;

public:
	// ──────────────────────────────────────────────────────
	// Player 전용 메서드
	// ──────────────────────────────────────────────────────

	// 패킷 핸들러 (GameThread에서 실행됨)
	void HandleSetDestPos(const game::Pos& cur_pos, const game::Pos& dest_pos);
	void HandleAttack(const game::Pos& cur_pos, int32_t target_id);

	// 패킷 전송 (User를 통해)
	template<typename T>
	void SendPacket(const T& packet)
	{
		if (owner_)
		{
			owner_->SendPacket(packet);
		}
	}

	// 인접 9셀 내 모든 Player에게 브로드캐스트 (자신 포함)
	template<typename T>
	void BroadcastToScene(const T& packet)
	{
		if (!m_pScene)
		{
			return;
		}

		m_pScene->ForEachAdjacentObjects(GetX(), GetZ(), [&](GameObject* obj)
		{
			Player* player = dynamic_cast<Player*>(obj);
			if (player && player->owner_)
			{
				player->owner_->SendPacket(packet);
			}
		});
	}

	// 인접 9셀 내 다른 Player들에게 브로드캐스트 (자신 제외)
	template<typename T>
	void BroadcastToOthers(const T& packet)
	{
		if (!m_pScene)
			return;

		m_pScene->ForEachAdjacentObjects(GetX(), GetZ(), [&](GameObject* obj)
		{
			if (obj == this)
				return;

			Player* player = dynamic_cast<Player*>(obj);
			if (player && player->owner_)
			{
				player->owner_->SendPacket(packet);
			}
		});
	}

	// HP
	void TakeDamage(int32_t damage, Player* attacker);
	int32_t GetHp() const { return hp_; }
	int32_t GetMaxHp() const { return max_hp_; }
	bool IsDead() const { return hp_ <= 0; }

	// Getter
	User* GetOwner() const { return owner_; }
	uint32_t GetPlayerId() const { return player_id_; }

	// 위치 정보 (MoveComponent에서 가져와 game::Pos로 변환)
	game::Pos GetCurrentPos() const;
	game::Pos GetDestPos() const;
	float GetAngle() const;

	// Component 직접 접근
	MoveComponent* GetMoveComponent() const { return m_pMoveComp; }
	AttackComponent* GetAttackComponent() const { return m_pAttackComp; }

private:
	void RejectAttack(const game::Pos& cur_pos);

	// ──────────────────────────────────────────────────────
	// 공격 이벤트 핸들러
	// ──────────────────────────────────────────────────────
	void HandleDamageApply(int32_t target_id, int32_t damage);

	// ──────────────────────────────────────────────────────
	// 이동 이벤트 핸들러
	// ──────────────────────────────────────────────────────
	void BroadcastMoveNotify();
	void BroadcastMoveStopNotify();

private:
	// 공격 사거리
	static constexpr float ATTACK_RANGE = 7.0f;

	User* owner_;               // 소유 네트워크 세션
	uint32_t player_id_;        // 플레이어 ID

	// HP
	int32_t hp_{10};
	int32_t max_hp_{10};

	// 컴포넌트 (Entity가 소유권 관리)
	MoveComponent* m_pMoveComp;
	AttackComponent* m_pAttackComp;

	// 이벤트 구독 (RAII 자동 해제)
	Subscription m_moveStartSub;
	Subscription m_arrivedSub;
	Subscription m_damageApplySub;
};
