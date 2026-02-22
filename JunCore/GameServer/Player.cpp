#include "Player.h"
#include "../JunCore/logic/GameScene.h"

Player::Player(GameScene* scene, User* owner, uint32_t player_id, float x, float y, float z)
	: GameObject(scene, x, y, z)
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
		BroadcastMoveStopNotify();
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
	int32_t scene_id = m_pScene ? m_pScene->GetId() : 0;
	float spawn_x = GetX();
	float spawn_y = GetY();
	float spawn_z = GetZ();

	// 스폰 위치로 MoveComponent에 위치 설정 (GameObject::SetPosition도 함께 갱신)
	m_pMoveComp->SetPosition(spawn_x, spawn_y, spawn_z);
	m_pMoveComp->Stop();

	LOG_INFO("[Player::OnEnter] Player (ID: %u) entered Scene %d at (%.2f, %.2f, %.2f)",
		player_id_,
		scene_id,
		spawn_x, spawn_y, spawn_z);

	// SCENE_ENTER_NOTIFY 전송 (Scene Enter 완료 알림)
	game::GC_SCENE_ENTER_NOTIFY enter_notify;
	enter_notify.set_scene_id(scene_id);
	enter_notify.set_player_id(player_id_);
	enter_notify.mutable_spawn_pos()->CopyFrom(GetCurrentPos());
	SendPacket(enter_notify);

	// 인접 셀(내 셀 포함)의 플레이어들에게 내 appear 알림 전송
	if (m_pScene)
	{
		game::GC_PLAYER_APPEAR_NOTIFY my_appear_notify;
		auto* my_info = my_appear_notify.add_players();
		my_info->set_player_id(player_id_);
		my_info->mutable_position()->CopyFrom(GetCurrentPos());
		my_info->set_angle(GetAngle());
		my_info->set_hp(hp_);
		my_info->set_max_hp(max_hp_);
		my_info->mutable_dest_pos()->CopyFrom(GetDestPos());

		m_pScene->ForEachAdjacentObjects(GetX(), GetZ(), [&](GameObject* obj)
			{
				if (obj == this)
				{
					return;
				}

				Player* other = dynamic_cast<Player*>(obj);
				if (other && other->owner_)
				{
					other->owner_->SendPacket(my_appear_notify);
				}
			});
	}
}

void Player::OnExit()
{
	LOG_INFO("[Player::OnExit] Player (ID: %u) left scene", player_id_);
}

void Player::OnAppear(std::vector<GameObject*>& others)
{
	game::GC_PLAYER_APPEAR_NOTIFY notify;
	for (auto* obj : others)
	{
		Player* other = dynamic_cast<Player*>(obj);
		if (other && other->owner_)
		{
			auto* info = notify.add_players();
			info->set_player_id(other->player_id_);
			info->mutable_position()->CopyFrom(other->GetCurrentPos());
			info->set_angle(other->GetAngle());
			info->set_hp(other->hp_);
			info->set_max_hp(other->max_hp_);
			info->mutable_dest_pos()->CopyFrom(other->GetDestPos());
		}
	}

	if (notify.players_size() > 0)
	{
		SendPacket(notify);
	}
}

void Player::OnDisappear(std::vector<GameObject*>& others)
{
	for (auto* obj : others)
	{
		Player* other = dynamic_cast<Player*>(obj);
		if (other)
		{
			game::GC_PLAYER_DISAPPEAR_NOTIFY notify;
			notify.set_player_id(other->player_id_);
			SendPacket(notify);
		}
	}
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
		RejectAttack(cur_pos);
		return;
	}

	if (target->IsDead())
	{
		LOG_WARN("[Player::HandleAttack] Player (ID: %u) attack failed: target (ID: %d) is dead",
			player_id_, target_id);
		RejectAttack(cur_pos);
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
		RejectAttack(cur_pos);
		return;
	}

	// 3. 클라-서버 위치 동기화
	if (!m_pMoveComp->TrySyncPosition(cur_pos.x(), 0.0f, cur_pos.z()))
	{
		game::GC_POSITION_SYNC_NOTIFY sync;
		sync.mutable_position()->CopyFrom(GetCurrentPos());
		SendPacket(sync);

		LOG_WARN("[Player::HandleAttack] Position sync: Player (ID: %u) client=(%.2f, %.2f) server=(%.2f, %.2f)",
			player_id_, cur_pos.x(), cur_pos.z(),
			m_pMoveComp->GetX(), m_pMoveComp->GetZ());
	}

	// 4. 이동 중지 (이벤트 발행 없이 즉시 정지 - GC_ATTACK_NOTIFY가 정지를 대신 알림)
	m_pMoveComp->Stop();

	// 5. 공격 상태 시작 (몇초 후 (ex. 칼을 내리쳤을때) 데미지 적용)
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

	// 공격자 사망 체크 (딜레이 동안 죽었을 수 있음)
	if (IsDead())
	{
		LOG_DEBUG("[Player::HandleDamageApply] Player (ID: %u) damage skipped: attacker is dead", player_id_);
		return;
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

	// 사망 시 진행 중이던 공격 취소
	if (IsDead())
	{
		m_pAttackComp->CancelAttack();
	}
}

void Player::RejectAttack(const game::Pos& cur_pos)
{
	// 공격 거부 시 클라-서버 위치 동기화 + 정지
	// 클라는 이미 이동을 멈추고 공격 시도했으므로, 서버도 정지 + 위치 보정
	if (!m_pMoveComp->TrySyncPosition(cur_pos.x(), 0.0f, cur_pos.z()))
	{
		game::GC_POSITION_SYNC_NOTIFY sync;
		sync.mutable_position()->CopyFrom(GetCurrentPos());
		SendPacket(sync);
	}

	m_pMoveComp->Stop();

	// 클라에게 정지 알림
	BroadcastMoveStopNotify();
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

	// 클라-서버 위치 동기화
	if (!m_pMoveComp->TrySyncPosition(cur_pos.x(), 0.0f, cur_pos.z()))
	{
		game::GC_POSITION_SYNC_NOTIFY sync;
		sync.mutable_position()->CopyFrom(GetCurrentPos());
		SendPacket(sync);

		LOG_WARN("[Player::HandleSetDestPos] Position sync: Player (ID: %u) client=(%.2f, %.2f) server=(%.2f, %.2f)",
			player_id_, cur_pos.x(), cur_pos.z(),
			m_pMoveComp->GetX(), m_pMoveComp->GetZ());
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

	// AOI 진단: GetNearbyObjects가 실제로 몇 명을 찾는지 확인
	size_t nearby_count = m_pScene ? m_pScene->GetNearbyObjects(GetX(), GetZ()).size() : 0;
	LOG_DEBUG("[Player::BroadcastMoveNotify] Player (ID: %u) nearby=%zu, "
		"MoveComp pos=(%.2f, %.2f) GameObj pos=(%.2f, %.2f) -> dest=(%.2f, %.2f)",
		player_id_, nearby_count,
		m_pMoveComp->GetX(), m_pMoveComp->GetZ(),
		GetX(), GetZ(),
		m_pMoveComp->GetDestX(), m_pMoveComp->GetDestZ());
}

void Player::BroadcastMoveStopNotify()
{
	game::GC_MOVE_STOP_NOTIFY notify;
	notify.set_player_id(player_id_);
	notify.mutable_position()->CopyFrom(GetCurrentPos());

	BroadcastToScene(notify);

	LOG_DEBUG("[Player::BroadcastMoveStopNotify] Player (ID: %u) stopped at (%.2f, %.2f, %.2f)",
		player_id_,
		m_pMoveComp->GetX(), m_pMoveComp->GetY(), m_pMoveComp->GetZ());
}
