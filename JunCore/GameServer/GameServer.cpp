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

	RegisterPacketHandler<game::CG_GAME_START_REQ>([this](User& user, const game::CG_GAME_START_REQ& request)
	{
		this->HandleGameStartRequest(user, request);
	});

	RegisterPacketHandler<game::CG_SCENE_ENTER_REQ>([this](User& user, const game::CG_SCENE_ENTER_REQ& request)
	{
		this->HandleSceneEnterRequest(user, request);
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
	if (user.GetPlayer() != nullptr)
	{
		SendError(user, game::ErrorCode::ALREADY_LOGGED_IN, "Already logged in");
		return;
	}

	// Player ID 생성 및 마지막 위치 조회
	const uint32_t player_id = GeneratePlayerId();
	const int32_t last_scene_id = GetDefaultSceneId();

	// User에 정보 저장
	user.SetPlayerId(player_id);
	user.SetLastSceneId(last_scene_id);

	game::GC_LOGIN_RES response;
	response.set_player_id(player_id);
	response.set_last_scene_id(last_scene_id);
	user.SendPacket(response);

	LOG_INFO("[LOGIN] Player ID %u assigned", player_id);
}

void GameServer::HandleGameStartRequest(User& user, const game::CG_GAME_START_REQ& request)
{
	// 이미 게임 중인지 확인
	if (user.GetPlayer() != nullptr)
	{
		SendError(user, game::ErrorCode::ALREADY_IN_GAME, "Already in game");
		return;
	}

	// LOGIN에서 발급받은 player_id와 last_scene_id 가져오기
	const uint32_t player_id = user.GetPlayerId();
	const int32_t last_scene_id = user.GetLastSceneId();

	// Scene 검증
	GameScene* target_scene = GetSceneById(last_scene_id);
	if (target_scene == nullptr)
	{
		SendError(user, game::ErrorCode::SCENE_NOT_FOUND, "Scene not found for scene id: " + std::to_string(last_scene_id));
		return;
	}

	LOG_INFO("[GAME_START] Starting game at Scene %d with Player ID %u", last_scene_id, player_id);

	// 1. RES 전송
	game::GC_GAME_START_RES response;
	user.SendPacket(response);

	// 2. Player 생성 (GameObject::Create가 자동으로 Scene Enter Job 등록)
	// Player::OnEnter에서 GC_SCENE_ENTER_NOTIFY 전송됨
	Player* player = GameObject::Create<Player>(target_scene, &user, player_id);
	user.SetPlayer(player);
}

// 구현 필요
void GameServer::HandleSceneEnterRequest(User& user, const game::CG_SCENE_ENTER_REQ& request)
{
	LOG_INFO("[SCENE_ENTER] Requesting Scene %d", request.scene_id());

	Player* player = user.GetPlayer();

	// Player가 없으면 GAME_START를 밟지 않은상태
	if (player == nullptr)
	{
		SendError(user, game::ErrorCode::PLAYER_NOT_FOUND, "Use GAME_START first");
		return;
	}

	// Scene 이동 (기존 Player)
	// 클라이언트 요청 검증
	GameScene* target_scene = GetSceneById(request.scene_id());
	if (target_scene == nullptr)
	{
		SendError(user, game::ErrorCode::INVALID_MAP_INDEX, "Invalid scene id: " + std::to_string(request.scene_id()));
		return;
	}

	LOG_INFO("[SCENE_ENTER] Moving Player (ID: %u) to Scene %d", player->GetPlayerId(), request.scene_id());

	// TODO: MoveToScene 구현 필요 (현재 Player는 Scene 이동 기능 없음)
	// player->MoveToScene(target_scene);
	// MoveToScene이 OnEnter를 호출하면 GC_SCENE_ENTER_NOTIFY가 자동 전송됨
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
	LOG_INFO("[CONNECT] User 0x%llX connected", (uintptr_t)user);
}

void GameServer::OnUserDisconnect(User* user)
{
	currentSessions_--;
	totalDisconnected_++;

	Player* player = user->GetPlayer();
	if (player == nullptr)
	{
		LOG_INFO("[DISCONNECT] User 0x%llX disconnected (not logged in)", (uintptr_t)user);
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
