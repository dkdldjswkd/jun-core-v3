#include "Player.h"
#include "../JunCore/logic/GameScene.h"

Player::Player(GameScene* scene, User* owner, uint32_t player_id)
	: GameObject(scene)
	, owner_(owner)
	, player_id_(player_id)
	, m_pMoveComp(nullptr)
	, m_pAttackComp(nullptr)
{
	// MoveComponent 추가 (Entity가 소유권 관리)
	m_pMoveComp = AddComponent<MoveComponent>(0.1f);  // 50Hz 기준 초당 5m 이동

	// AttackComponent 추가
	m_pAttackComp = AddComponent<AttackComponent>();

	// 이동 이벤트 구독 (RAII - Player 소멸 시 자동 해제)
	m_moveStartSub = m_pMoveComp->OnMoveStart.Subscribe([this]()
	{
		BroadcastMoveNotify();
	});

	m_arrivedSub = m_pMoveComp->OnArrived.Subscribe([this]()
	{
		BroadcastMoveNotify();
	});

	// 공격 데미지 적용 이벤트 구독
	m_damageApplySub = m_pAttackComp->OnDamageApply.Subscribe([this](int32_t target_id, int32_t damage)
	{
		HandleDamageApply(target_id, damage);
	});
}

Player::~Player()
{
	// m_pMoveComp는 Entity가 unique_ptr로 관리하므로 별도 삭제 불필요
}

void Player::OnUpdate()
{
	// Update는 렌더링 관련 처리
	// 현재는 사용하지 않음
}

void Player::OnFixedUpdate()
{
	// ──────────────────────────────────────────────────────
	// GameThread에서 고정 간격으로 호출됨 (50Hz 기본)
	// FixedUpdate는 항상 동일한 간격으로 호출됨을 보장
	// ──────────────────────────────────────────────────────

	// 모든 컴포넌트의 FixedUpdate 호출 (MoveComponent 포함)
	// MoveComponent가 이동 시작/도착 시 이벤트 발행 → BroadcastMoveNotify 호출됨
	FixedUpdateComponents();
}

void Player::OnEnter()
{
	// User에서 scene_id와 spawn_pos 가져오기
	int32_t scene_id = 0;
	float spawn_x = 0.0f, spawn_y = 0.0f, spawn_z = 0.0f;

	if (owner_)
	{
		scene_id = owner_->GetLastSceneId();
		owner_->GetSpawnPos(spawn_x, spawn_y, spawn_z);
	}

	// 스폰 위치로 MoveComponent에 위치 설정
	m_pMoveComp->SetPosition(spawn_x, spawn_y, spawn_z);
	m_pMoveComp->SetDestination(spawn_x, spawn_y, spawn_z);

	LOG_INFO("[Player::OnEnter] Player (ID: %u) entered Scene %d at (%.2f, %.2f, %.2f)",
		player_id_,
		scene_id,
		spawn_x, spawn_y, spawn_z);

	// 1. SCENE_ENTER_NOTIFY 전송 (Scene Enter 완료 알림)
	game::GC_SCENE_ENTER_NOTIFY enter_notify;
	enter_notify.set_scene_id(scene_id);
	enter_notify.set_player_id(player_id_);
	enter_notify.mutable_spawn_pos()->CopyFrom(GetCurrentPos());
	SendPacket(enter_notify);

	// 2. 주변 플레이어들에게 내가 나타났다고 알림
	game::GC_PLAYER_APPEAR_NOTIFY my_appear;
	my_appear.set_player_id(player_id_);
	my_appear.mutable_position()->CopyFrom(GetCurrentPos());
	my_appear.set_angle(GetAngle());
	my_appear.set_hp(hp_);
	my_appear.set_max_hp(max_hp_);

	BroadcastToOthers(my_appear);

	LOG_DEBUG("[Player::OnEnter] Broadcast APPEAR - Player (ID: %u) to other players",
		player_id_);

	// 3. 나에게 주변에 이미 있는 모든 플레이어 정보 전송
	if (!m_pScene)
		return;

	const auto& objects = m_pScene->GetObjects();
	int other_player_count = 0;
	for (auto* obj : objects)
	{
		Player* other = dynamic_cast<Player*>(obj);
		if (other && other != this && other->owner_)
		{
			game::GC_PLAYER_APPEAR_NOTIFY other_appear;
			other_appear.set_player_id(other->player_id_);
			other_appear.mutable_position()->CopyFrom(other->GetCurrentPos());
			other_appear.set_angle(other->GetAngle());
			other_appear.set_hp(other->hp_);
			other_appear.set_max_hp(other->max_hp_);

			SendPacket(other_appear);
			other_player_count++;
		}
	}

	if (other_player_count > 0)
	{
		LOG_DEBUG("[Player::OnEnter] Sent %d existing players info to Player (ID: %u)",
			other_player_count, player_id_);
	}
}

void Player::OnExit()
{
	LOG_INFO("[Player::OnExit] Player (ID: %u) left scene", player_id_);

	// 주변 플레이어들에게 내가 사라졌다고 알림
	game::GC_PLAYER_DISAPPEAR_NOTIFY disappear;
	disappear.set_player_id(player_id_);

	BroadcastToOthers(disappear);

	LOG_DEBUG("[Player::OnExit] Broadcast DISAPPEAR - Player (ID: %u) to other players",
		player_id_);

}

void Player::PostSetDestPosJob(const game::Pos& cur_pos, const game::Pos& dest_pos)
{
	PostJob([this, cur_pos, dest_pos]()
	{
		this->HandleSetDestPos(cur_pos, dest_pos);
	});
}

void Player::PostAttackJob(const game::Pos& cur_pos, int32_t target_id)
{
	PostJob([this, cur_pos, target_id]()
	{
		this->HandleAttack(cur_pos, target_id);
	});
}

void Player::HandleAttack(const game::Pos& cur_pos, int32_t target_id)
{
	// 0. 사망 상태 체크
	if (IsDead())
	{
		LOG_WARN("[Player::HandleAttack] Player (ID: %u) attack failed: attacker is dead", player_id_);
		return;
	}

	// 1. 타겟 검증: Scene에서 target_id로 Player 찾기
	Player* target = nullptr;
	if (m_pScene)
	{
		const auto& objects = m_pScene->GetObjects();
		for (auto* obj : objects)
		{
			Player* p = dynamic_cast<Player*>(obj);
			if (p && p->GetPlayerId() == static_cast<uint32_t>(target_id))
			{
				target = p;
				break;
			}
		}
	}

	if (!target)
	{
		LOG_WARN("[Player::HandleAttack] Player (ID: %u) attack failed: target (ID: %d) not found",
			player_id_, target_id);
		return;
	}

	if (target->IsDead())
	{
		LOG_WARN("[Player::HandleAttack] Player (ID: %u) attack failed: target (ID: %d) is dead",
			player_id_, target_id);
		return;
	}

	// 2. 공격 사거리 검증 (서버 위치 기준, ATTACK_RANGE * 1.2 러프 체크)
	float atk_dx = m_pMoveComp->GetX() - target->GetMoveComponent()->GetX();
	float atk_dz = m_pMoveComp->GetZ() - target->GetMoveComponent()->GetZ();
	float atkDistSq = atk_dx * atk_dx + atk_dz * atk_dz;
	float toleranceRange = ATTACK_RANGE * 1.2f;

	if (atkDistSq > toleranceRange * toleranceRange)
	{
		LOG_WARN("[Player::HandleAttack] Player (ID: %u) attack rejected: target (ID: %d) out of range (dist=%.2f, tolerance=%.2f)",
			player_id_, target_id, std::sqrt(atkDistSq), toleranceRange);
		return;
	}

	// 3. 클라-서버 위치 오차 체크 (cur_pos sync)
	float dx = cur_pos.x() - m_pMoveComp->GetX();
	float dz = cur_pos.z() - m_pMoveComp->GetZ();
	float distSq = dx * dx + dz * dz;

	if (distSq <= POSITION_SYNC_THRESHOLD * POSITION_SYNC_THRESHOLD)
	{
		m_pMoveComp->SetPosition(cur_pos.x(), 0.0f, cur_pos.z());
	}
	else
	{
		game::GC_POSITION_SYNC_NOTIFY sync;
		sync.mutable_position()->CopyFrom(GetCurrentPos());
		SendPacket(sync);

		LOG_WARN("[Player::HandleAttack] Position sync: Player (ID: %u) client=(%.2f, %.2f) server=(%.2f, %.2f)",
			player_id_, cur_pos.x(), cur_pos.z(),
			m_pMoveComp->GetX(), m_pMoveComp->GetZ());
	}

	// 4. 이동 중지 (현재 서버 위치에서 멈춤)
	m_pMoveComp->SetDestination(m_pMoveComp->GetX(), 0.0f, m_pMoveComp->GetZ());

	// 5. 공격 상태 시작 (0.5초 후 데미지 적용)
	m_pAttackComp->StartAttack(target_id);

	// 6. 공격 알림 브로드캐스트 (클라이언트 애니메이션 트리거)
	game::GC_ATTACK_NOTIFY notify;
	notify.set_attacker_id(player_id_);
	notify.set_target_id(target_id);
	notify.mutable_attacker_pos()->CopyFrom(GetCurrentPos());

	BroadcastToScene(notify);

	LOG_DEBUG("[Player::HandleAttack] Player (ID: %u) attacks Player (ID: %d) at (%.2f, %.2f) dist=%.2f",
		player_id_, target_id, m_pMoveComp->GetX(), m_pMoveComp->GetZ(), std::sqrt(atkDistSq));
}

void Player::HandleDamageApply(int32_t target_id, int32_t damage)
{
	// AttackComponent의 OnDamageApply 이벤트에서 호출됨
	// 0.5초 딜레이 후 실제 데미지 적용 시점

	// 타겟 재검증 (딜레이 동안 나갔을 수 있음)
	Player* target = nullptr;
	if (m_pScene)
	{
		const auto& objects = m_pScene->GetObjects();
		for (auto* obj : objects)
		{
			Player* p = dynamic_cast<Player*>(obj);
			if (p && p->GetPlayerId() == static_cast<uint32_t>(target_id))
			{
				target = p;
				break;
			}
		}
	}

	if (!target || target->IsDead())
	{
		LOG_DEBUG("[Player::HandleDamageApply] Player (ID: %u) damage skipped: target (ID: %d) %s",
			player_id_, target_id, !target ? "not found" : "is dead");
		return;
	}

	// 데미지 적용
	target->TakeDamage(damage, this);

	// 데미지 알림 브로드캐스트
	game::GC_DAMAGE_NOTIFY damage_notify;
	damage_notify.set_attacker_id(player_id_);
	damage_notify.set_target_id(target_id);
	damage_notify.set_damage(damage);
	damage_notify.set_target_hp(target->GetHp());

	BroadcastToScene(damage_notify);

	LOG_DEBUG("[Player::HandleDamageApply] Player (ID: %u) dealt %d damage to Player (ID: %d), remaining HP: %d",
		player_id_, damage, target_id, target->GetHp());
}

void Player::TakeDamage(int32_t damage, Player* attacker)
{
	if (IsDead())
	{
		return;
	}

	hp_ -= damage;
	if (hp_ < 0)
	{
		hp_ = 0;
	}

	LOG_INFO("[Player::TakeDamage] Player (ID: %u) took %d damage from Player (ID: %u), HP: %d/%d",
		player_id_, damage,
		attacker ? attacker->GetPlayerId() : 0,
		hp_, max_hp_);
}

void Player::HandleSetDestPos(const game::Pos& cur_pos, const game::Pos& dest_pos)
{
	// ──────────────────────────────────────────────────────
	// GameThread에서 실행되는 목표 위치 설정
	// ──────────────────────────────────────────────────────

	// 이동 시 공격 취소
	if (m_pAttackComp->IsAttacking())
	{
		m_pAttackComp->CancelAttack();
		LOG_DEBUG("[Player::HandleSetDestPos] Player (ID: %u) attack cancelled by movement", player_id_);
	}

	// 클라-서버 위치 오차 체크
	float dx = cur_pos.x() - m_pMoveComp->GetX();
	float dz = cur_pos.z() - m_pMoveComp->GetZ();
	float distSq = dx * dx + dz * dz;

	if (distSq <= POSITION_SYNC_THRESHOLD * POSITION_SYNC_THRESHOLD)
	{
		// 임계값 이내 → 클라 cur_pos로 위치 보정
		m_pMoveComp->SetPosition(cur_pos.x(), 0.0f, cur_pos.z());
	}
	else
	{
		// 임계값 초과 → 서버 위치 유지, 클라에 위치 보정 알림
		game::GC_POSITION_SYNC_NOTIFY sync;
		sync.mutable_position()->CopyFrom(GetCurrentPos());
		SendPacket(sync);

		LOG_WARN("[Player::HandleSetDestPos] Position sync: Player (ID: %u) client=(%.2f, %.2f) server=(%.2f, %.2f) dist=%.2f",
			player_id_, cur_pos.x(), cur_pos.z(),
			m_pMoveComp->GetX(), m_pMoveComp->GetZ(),
			std::sqrt(distSq));
	}

	m_pMoveComp->SetDestination(dest_pos.x(), dest_pos.y(), dest_pos.z());

	LOG_DEBUG("[Player::HandleSetDestPos] Player (ID: %u) dest pos set to (%.2f, %.2f, %.2f)",
		player_id_,
		dest_pos.x(), dest_pos.y(), dest_pos.z());
}

game::Pos Player::GetCurrentPos() const
{
	game::Pos pos;
	pos.set_x(m_pMoveComp->GetX());
	pos.set_y(m_pMoveComp->GetY());
	pos.set_z(m_pMoveComp->GetZ());
	return pos;
}

game::Pos Player::GetDestPos() const
{
	game::Pos pos;
	pos.set_x(m_pMoveComp->GetDestX());
	pos.set_y(m_pMoveComp->GetDestY());
	pos.set_z(m_pMoveComp->GetDestZ());
	return pos;
}

float Player::GetAngle() const
{
	return m_pMoveComp->GetAngle();
}

void Player::BroadcastMoveNotify()
{
	game::GC_MOVE_NOTIFY notify;
	notify.set_player_id(player_id_);
	notify.mutable_cur_pos()->CopyFrom(GetCurrentPos());
	notify.mutable_move_pos()->CopyFrom(GetDestPos());

	BroadcastToScene(notify);

	LOG_DEBUG("[Player::BroadcastMoveNotify] Player (ID: %u) pos: (%.2f, %.2f, %.2f) -> dest: (%.2f, %.2f, %.2f)",
		player_id_,
		m_pMoveComp->GetX(), m_pMoveComp->GetY(), m_pMoveComp->GetZ(),
		m_pMoveComp->GetDestX(), m_pMoveComp->GetDestY(), m_pMoveComp->GetDestZ());
}
