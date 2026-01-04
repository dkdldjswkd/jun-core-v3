#include "Player.h"
#include "../JunCore/logic/GameScene.h"
#include <cmath>

Player::Player(User* owner, const std::string& username)
	: owner_(owner)
	, username_(username)
	, player_id_(0)  // TODO: ID 생성 로직
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

		// TODO: 주변 플레이어에게 주기적으로 브로드캐스트
		// 예: 0.1초마다 한 번씩
	}
}

void Player::OnEnter(GameScene* scene)
{
	scene_ = scene;

	// Scene 입장 시 초기화
	LOG_INFO("[Player::OnEnter] Player %s entered scene at (%.2f, %.2f, %.2f)",
		username_.c_str(),
		currentPos_.x(), currentPos_.y(), currentPos_.z());

	// TODO: 주변 플레이어 정보 전송
}

void Player::OnExit(GameScene* scene)
{
	scene_ = nullptr;

	// TODO: 주변 플레이어에게 퇴장 알림
	LOG_INFO("[Player::OnExit] Player %s left scene", username_.c_str());
}

void Player::PostSetDestPosJob(const game::Pos& dest_pos)
{
	// 목표 위치 설정 Job을 JobObject 큐에 등록
	// LogicThread에서 실행됨
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

	destPos_.set_x(dest_pos.x());
	destPos_.set_y(dest_pos.y());
	destPos_.set_z(dest_pos.z());

	LOG_DEBUG("[Player::HandleSetDestPos] Player %s dest pos set to (%.2f, %.2f, %.2f)",
		username_.c_str(),
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
