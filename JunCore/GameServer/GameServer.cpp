#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "GameServer.h"
#include "Player.h"
#include "../JunCore/logic/GameScene.h"
#include "../JunCore/logic/LogicThread.h"

GameServer::GameServer(std::shared_ptr<IOCPManager> manager, int logic_thread_count)
	: Server(manager, logic_thread_count)
{
}

GameServer::~GameServer()
{
}

void GameServer::RegisterPacketHandlers()
{
	RegisterPacketHandler<game::CG_LOGIN_REQ>([this](User& user, const game::CG_LOGIN_REQ& request)
	{
		this->HandleLoginRequest(user, request);
	});

	RegisterPacketHandler<game::CG_SCENE_READY_REQ>([this](User& user, const game::CG_SCENE_READY_REQ& request)
	{
		this->HandleSceneReadyRequest(user, request);
	});

	RegisterPacketHandler<game::CG_SCENE_CHANGE_REQ>([this](User& user, const game::CG_SCENE_CHANGE_REQ& request)
	{
		this->HandleSceneChangeRequest(user, request);
	});

	RegisterPacketHandler<game::CG_MOVE_REQ>([this](User& user, const game::CG_MOVE_REQ& request)
	{
		this->HandleMoveRequest(user, request);
	});
}

void GameServer::SendError(User& user, game::ErrorCode error_code, const std::string& error_message)
{
	game::GC_ERROR_NOTIFY error_notify;
	error_notify.set_error_code(error_code);
	error_notify.set_error_message(error_message);
	user.SendPacket(error_notify);
}

void GameServer::HandleLoginRequest(User& user, const game::CG_LOGIN_REQ& request)
{
	// 이미 로그인된 상태인지 확인
	if (user.GetPlayerId() != 0)
	{
		SendError(user, game::ErrorCode::ALREADY_LOGGED_IN, "Already logged in");
		return;
	}

	// TODO: 실제 인증 로직 (DB 조회 등)
	bool auth_success = true;
	if (!auth_success)
	{
		SendError(user, game::ErrorCode::AUTH_FAILED, "Authentication failed");
		return;
	}

	// Player ID 생성 및 마지막 위치 조회
	const uint32_t player_id = GeneratePlayerId();
	const int32_t scene_id = GetDefaultSceneId();

	// TODO: DB에서 스폰 위치 조회 (현재는 기본값)
	const float spawn_x = 0.0f;
	const float spawn_y = 0.0f;
	const float spawn_z = 0.0f;

	// User에 정보 저장 (씬 로딩 완료 후 사용)
	user.SetPlayerId(player_id);
	user.SetLastSceneId(scene_id);
	user.SetSpawnPos(spawn_x, spawn_y, spawn_z);

	// 클라이언트에게 로그인 정보 전송 (씬 로딩용)
	game::GC_LOGIN_RES response;
	response.set_player_id(player_id);
	response.set_scene_id(scene_id);
	response.mutable_spawn_pos()->set_x(spawn_x);
	response.mutable_spawn_pos()->set_y(spawn_y);
	response.mutable_spawn_pos()->set_z(spawn_z);
	user.SendPacket(response);

	LOG_INFO("[LOGIN] Player ID %u assigned, Scene %d, SpawnPos (%.2f, %.2f, %.2f)",
		player_id, scene_id, spawn_x, spawn_y, spawn_z);
}

void GameServer::HandleSceneReadyRequest(User& user, const game::CG_SCENE_READY_REQ& request)
{
	// 로그인 확인
	if (user.GetPlayerId() == 0)
	{
		SendError(user, game::ErrorCode::NOT_LOGGED_IN, "Not logged in");
		return;
	}

	// 이미 게임 중인 경우 (맵 이동 후 로딩 완료)
	Player* player = user.GetPlayer();
	if (player != nullptr)
	{
		// 맵 이동 후 로딩 완료 - MoveToScene이 대기 중인 상태
		// TODO: 현재는 첫 진입만 지원, 맵 이동은 추후 구현
		LOG_INFO("[SCENE_READY] Player (ID: %u) scene ready after map change", player->GetPlayerId());
		return;
	}

	// 첫 진입 (로그인 후 첫 씬 로딩 완료)
	const uint32_t player_id = user.GetPlayerId();
	const int32_t scene_id = user.GetLastSceneId();

	// Scene 검증
	GameScene* target_scene = GetSceneById(scene_id);
	if (target_scene == nullptr)
	{
		SendError(user, game::ErrorCode::SCENE_NOT_FOUND, "Scene not found: " + std::to_string(scene_id));
		return;
	}

	LOG_INFO("[SCENE_READY] Creating Player (ID: %u) at Scene %d", player_id, scene_id);

	// Player 생성 (GameObject::Create가 자동으로 Scene Enter Job 등록)
	// Player::OnEnter에서 GC_SCENE_ENTER_NOTIFY 전송됨
	player = GameObject::Create<Player>(target_scene, &user, player_id);
	user.SetPlayer(player);
}

void GameServer::HandleSceneChangeRequest(User& user, const game::CG_SCENE_CHANGE_REQ& request)
{
	Player* player = user.GetPlayer();
	if (player == nullptr)
	{
		SendError(user, game::ErrorCode::PLAYER_NOT_FOUND, "Player not found");
		return;
	}

	const int32_t new_scene_id = request.scene_id();

	// Scene 검증
	GameScene* target_scene = GetSceneById(new_scene_id);
	if (target_scene == nullptr)
	{
		SendError(user, game::ErrorCode::SCENE_NOT_FOUND, "Scene not found: " + std::to_string(new_scene_id));
		return;
	}

	// TODO: Scene 이동 조건 검증 (레벨, 퀘스트 조건 등)

	// TODO: DB에서 새 씬의 스폰 위치 조회 (현재는 기본값)
	const float spawn_x = 0.0f;
	const float spawn_y = 0.0f;
	const float spawn_z = 0.0f;

	// User에 새 씬 정보 저장
	user.SetLastSceneId(new_scene_id);
	user.SetSpawnPos(spawn_x, spawn_y, spawn_z);

	LOG_INFO("[SCENE_CHANGE] Player (ID: %u) changing to Scene %d", player->GetPlayerId(), new_scene_id);

	// 클라이언트에게 씬 변경 승인 (새 씬 로딩 시작)
	game::GC_SCENE_CHANGE_RES response;
	response.set_scene_id(new_scene_id);
	response.mutable_spawn_pos()->set_x(spawn_x);
	response.mutable_spawn_pos()->set_y(spawn_y);
	response.mutable_spawn_pos()->set_z(spawn_z);
	user.SendPacket(response);

	// Player를 새 Scene으로 이동 (현재 Scene에서 Exit, 새 Scene에서 Enter)
	player->MoveToScene(target_scene);
}

void GameServer::HandleMoveRequest(User& user, const game::CG_MOVE_REQ& request)
{
	Player* player = user.GetPlayer();
	if (player == nullptr)
	{
		SendError(user, game::ErrorCode::PLAYER_NOT_FOUND, "Player not found");
		return;
	}

	LOG_DEBUG("[MOVE] Player (ID: %u), cur_pos: (%.2f, %.2f, %.2f), move_pos: (%.2f, %.2f, %.2f)",
		player->GetPlayerId(),
		request.cur_pos().x(), request.cur_pos().y(), request.cur_pos().z(),
		request.move_pos().x(), request.move_pos().y(), request.move_pos().z());

	player->PostSetDestPosJob(request.move_pos());
}

void GameServer::OnSessionConnect(User* user)
{
	currentSessions_++;
	totalConnected_++;
	LOG_INFO("[CONNECT] New connection");
}

void GameServer::OnUserDisconnect(User* user)
{
	currentSessions_--;
	totalDisconnected_++;

	Player* player = user->GetPlayer();
	if (player == nullptr)
	{
		LOG_INFO("[DISCONNECT] User disconnected (not logged in)");
		return;
	}

	LOG_INFO("[DISCONNECT] Player (ID: %u) disconnected", player->GetPlayerId());

	player->ExitScene();
	// TODO: delete player - JobObject 생명주기 관리 정책 필요
	user->ClearPlayer();
}

void GameServer::OnServerStart()
{
	LOG_INFO("GameServer started successfully!");
	scene_ = std::make_unique<GameScene>(GetLogicThread(0));
}

void GameServer::OnServerStop()
{
	LOG_INFO("GameServer stopped successfully!");
	scene_.reset();
}

GameScene* GameServer::GetSceneById(int32_t scene_id)
{
	// TODO: Scene ID별 Scene 매핑 (현재는 단일 Scene만 지원)
	if (scene_id == 0)
	{
		return scene_.get();
	}
	return nullptr;
}

uint32_t GameServer::GeneratePlayerId()
{
	// TODO: DB에서 Player ID 조회/생성 (현재는 메모리에서 생성)
	return nextPlayerId_.fetch_add(1);
}
