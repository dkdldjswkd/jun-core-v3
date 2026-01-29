#include "Player.h"
#include "../JunCore/logic/GameScene.h"

Player::Player(GameScene* scene, User* owner, uint32_t player_id)
	: GameObject(scene)
	, owner_(owner)
	, player_id_(player_id)
	, m_pMoveComp(nullptr)
{
	// MoveComponent 추가 (Entity가 소유권 관리)
	m_pMoveComp = AddComponent<MoveComponent>(0.1f);  // 50Hz 기준 초당 5m 이동
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
	// LogicThread에서 고정 간격으로 호출됨 (50Hz 기본)
	// FixedUpdate는 항상 동일한 간격으로 호출됨을 보장
	// ──────────────────────────────────────────────────────

	// 모든 컴포넌트의 FixedUpdate 호출 (MoveComponent 포함)
	FixedUpdateComponents();

	// 매 프레임 모든 플레이어에게 위치 브로드캐스트 (디버깅용)
	game::GC_MOVE_NOTIFY notify;
	notify.set_player_id(player_id_);
	notify.mutable_cur_pos()->CopyFrom(GetCurrentPos());
	notify.mutable_move_pos()->CopyFrom(GetDestPos());

	BroadcastToScene(notify);

	LOG_DEBUG("[Player::OnFixedUpdate] Broadcast position - Player (ID: %u) pos: (%.2f, %.2f, %.2f) -> dest: (%.2f, %.2f, %.2f)",
		player_id_,
		m_pMoveComp->GetX(), m_pMoveComp->GetY(), m_pMoveComp->GetZ(),
		m_pMoveComp->GetDestX(), m_pMoveComp->GetDestY(), m_pMoveComp->GetDestZ());
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

void Player::PostSetDestPosJob(const game::Pos& dest_pos)
{
	PostJob([this, dest_pos]()
	{
		this->HandleSetDestPos(dest_pos);
	});
}

void Player::HandleSetDestPos(const game::Pos& dest_pos)
{
	// ──────────────────────────────────────────────────────
	// LogicThread에서 실행되는 목표 위치 설정
	// ──────────────────────────────────────────────────────

	// TODO: 이동 검증
	// - 목표 위치가 맵 범위 내인지
	// - 벽 충돌 체크 등

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
