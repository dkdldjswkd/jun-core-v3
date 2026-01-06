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

	RegisterPacketHandler<game::CG_MOVE_REQ>([this](User& user, const game::CG_MOVE_REQ& request)
	{
		this->HandleMoveRequest(user, request);
	});
}

void GameServer::HandleLoginRequest(User& user, const game::CG_LOGIN_REQ& request)
{
	if (user.GetPlayer() != nullptr)
	{
		LOG_WARN("[LOGIN] User 0x%llX already logged in", (uintptr_t)&user);
		return;
	}

	LOG_INFO("[LOGIN] User 0x%llX, username: %s", (uintptr_t)&user, request.username().c_str());

	// Player 생성 및 Scene Enter (Job으로 처리됨)
	GameScene* enter_scene = ChooseSceneForNewPlayer();
	Player* player = GameObject::Create<Player>(enter_scene, &user, request.username());
	user.SetPlayer(player);

	// 로그인 응답 전송 (NetworkThread에서 바로 전송 - 동시성 이슈 없음)
	game::GC_LOGIN_RES response;
	response.set_success(true);
	response.set_player_id(player->GetPlayerId());
	user.SendPacket(response);
}

void GameServer::HandleMoveRequest(User& user, const game::CG_MOVE_REQ& request)
{
	Player* player = user.GetPlayer();
	if (player == nullptr)
	{
		LOG_WARN("[MOVE] User 0x%llX not logged in", (uintptr_t)&user);
		return;
	}

	LOG_DEBUG("[MOVE] Player %s, cur_pos: (%.2f, %.2f, %.2f), move_pos: (%.2f, %.2f, %.2f)",
		player->GetUsername().c_str(),
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

	LOG_INFO("[DISCONNECT] Player %s (User 0x%llX) disconnected",
		player->GetUsername().c_str(), (uintptr_t)user);

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

GameScene* GameServer::ChooseSceneForNewPlayer()
{
	// TODO: 여러 Scene 지원하도록 확장
	return scene_.get();
}
