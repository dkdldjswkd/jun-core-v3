#include "Player.h"
#include "../JunCore/logic/GameScene.h"
#include <cmath>

Player::Player(GameScene* scene, User* owner, uint32_t player_id)
	: GameObject(scene)
	, owner_(owner)
	, player_id_(player_id)
{
	// 초기 위치 설정 (원점)
	currentPos_.set_x(0.0f);
	currentPos_.set_y(0.0f);
	currentPos_.set_z(0.0f);

	destPos_.set_x(0.0f);
	destPos_.set_y(0.0f);
	destPos_.set_z(0.0f);
}

Player::~Player()
{
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

	// 목표 위치까지 이동
	if (!HasReachedDestination())
	{
		MoveTowardsDestination();
	}

	// 매 프레임 모든 플레이어에게 위치 브로드캐스트 (디버깅용)
	game::GC_MOVE_NOTIFY notify;
	notify.set_player_id(player_id_);
	notify.mutable_cur_pos()->CopyFrom(currentPos_);
	notify.mutable_move_pos()->CopyFrom(destPos_);

	BroadcastToScene(notify);

	LOG_DEBUG("[Player::OnFixedUpdate] Broadcast position - Player (ID: %u) pos: (%.2f, %.2f, %.2f) -> dest: (%.2f, %.2f, %.2f)",
		player_id_,
		currentPos_.x(), currentPos_.y(), currentPos_.z(),
		destPos_.x(), destPos_.y(), destPos_.z());
}

void Player::OnEnter(GameScene* scene)
{
	scene_ = scene;

	// User에서 scene_id와 spawn_pos 가져오기
	int32_t scene_id = 0;
	float spawn_x = 0.0f, spawn_y = 0.0f, spawn_z = 0.0f;

	if (owner_)
	{
		scene_id = owner_->GetLastSceneId();
		owner_->GetSpawnPos(spawn_x, spawn_y, spawn_z);
	}

	// 스폰 위치로 현재 위치 설정
	currentPos_.set_x(spawn_x);
	currentPos_.set_y(spawn_y);
	currentPos_.set_z(spawn_z);
	destPos_.CopyFrom(currentPos_);

	LOG_INFO("[Player::OnEnter] Player (ID: %u) entered Scene %d at (%.2f, %.2f, %.2f)",
		player_id_,
		scene_id,
		currentPos_.x(), currentPos_.y(), currentPos_.z());

	// 1. SCENE_ENTER_NOTIFY 전송 (Scene Enter 완료 알림)
	game::GC_SCENE_ENTER_NOTIFY enter_notify;
	enter_notify.set_scene_id(scene_id);
	enter_notify.set_player_id(player_id_);
	enter_notify.mutable_spawn_pos()->CopyFrom(currentPos_);
	SendPacket(enter_notify);

	// 2. 주변 플레이어들에게 내가 나타났다고 알림
	game::GC_PLAYER_APPEAR_NOTIFY my_appear;
	my_appear.set_player_id(player_id_);
	my_appear.mutable_position()->CopyFrom(currentPos_);

	BroadcastToOthers(my_appear);

	LOG_DEBUG("[Player::OnEnter] Broadcast APPEAR - Player (ID: %u) to other players",
		player_id_);

	// 3. 나에게 주변에 이미 있는 모든 플레이어 정보 전송
	if (!scene_)
		return;

	const auto& objects = scene_->GetObjects();
	int other_player_count = 0;
	for (auto* obj : objects)
	{
		Player* other = dynamic_cast<Player*>(obj);
		if (other && other != this && other->owner_)
		{
			game::GC_PLAYER_APPEAR_NOTIFY other_appear;
			other_appear.set_player_id(other->player_id_);
			other_appear.mutable_position()->CopyFrom(other->currentPos_);

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

void Player::OnExit(GameScene* scene)
{
	LOG_INFO("[Player::OnExit] Player (ID: %u) left scene", player_id_);

	// 주변 플레이어들에게 내가 사라졌다고 알림
	game::GC_PLAYER_DISAPPEAR_NOTIFY disappear;
	disappear.set_player_id(player_id_);

	BroadcastToOthers(disappear);

	LOG_DEBUG("[Player::OnExit] Broadcast DISAPPEAR - Player (ID: %u) to other players",
		player_id_);

	scene_ = nullptr;
}

void Player::PostSetDestPosJob(const game::Pos& dest_pos)
{
	PostJob([this, dest_pos]()
	{
		this->HandleSetDestPos(dest_pos);
	});
}

void Player::MoveToScene(GameScene* new_scene)
{
	if (new_scene == nullptr)
	{
		return;
	}

	// Job #1: 현재 Scene에서 Exit + LogicThread 변경
	PostJob([this, new_scene]()
	{
		if (scene_ == new_scene)
		{
			return;
		}

		// 이전 Scene에서 Exit
		if (scene_)
		{
			scene_->Exit(this);
			OnExit(scene_);
		}

		// Scene 포인터 업데이트
		scene_ = new_scene;

		// LogicThread 변경 (JobObject::Flush가 자동으로 새 스레드에 이동)
		SetLogicThread(new_scene->GetLogicThread());
	});

	// Job #2: 새 Scene에서 Enter (새 LogicThread에서 실행됨)
	PostJob([this, new_scene]()
	{
		new_scene->Enter(this);
		OnEnter(new_scene);
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

	destPos_.set_x(dest_pos.x());
	destPos_.set_y(dest_pos.y());
	destPos_.set_z(dest_pos.z());

	LOG_DEBUG("[Player::HandleSetDestPos] Player (ID: %u) dest pos set to (%.2f, %.2f, %.2f)",
		player_id_,
		destPos_.x(), destPos_.y(), destPos_.z());
}

bool Player::HasReachedDestination() const
{
	// 거리 계산 (XZ 평면만 사용, Y는 높이이므로 제외)
	float dx = destPos_.x() - currentPos_.x();
	float dz = destPos_.z() - currentPos_.z();
	float distanceSq = dx * dx + dz * dz;

	// 0.1m 이내면 도착으로 간주
	const float ARRIVAL_THRESHOLD = 0.1f;
	return distanceSq < (ARRIVAL_THRESHOLD * ARRIVAL_THRESHOLD);
}

void Player::MoveTowardsDestination()
{
	// 방향 벡터 계산
	float dx = destPos_.x() - currentPos_.x();
	float dz = destPos_.z() - currentPos_.z();

	// 거리 계산
	float distance = std::sqrt(dx * dx + dz * dz);

	if (distance < 0.001f)
	{
		// 이미 도착
		return;
	}

	// 정규화된 방향 벡터
	float dirX = dx / distance;
	float dirZ = dz / distance;

	// 이동 거리 (moveSpeed는 FixedUpdate당 이동 거리)
	float moveDistance = moveSpeed_;

	// 목표 지점을 넘어가지 않도록 제한
	if (moveDistance > distance)
	{
		moveDistance = distance;
	}

	// 위치 업데이트
	currentPos_.set_x(currentPos_.x() + dirX * moveDistance);
	currentPos_.set_z(currentPos_.z() + dirZ * moveDistance);

	// Y축은 그대로 유지 (높이 변화 없음)
	// 추후 지형 높이에 따라 Y도 업데이트 필요
}
