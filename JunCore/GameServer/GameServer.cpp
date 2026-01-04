#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "GameServer.h"
#include "Player.h"
#include "../JunCore/logic/GameScene.h"

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
	// ══════════════════════════════════════════════════════
	// Worker Thread
	// ══════════════════════════════════════════════════════

	// 1. 중복 로그인 체크
	if (user.GetPlayer() != nullptr)
	{
		LOG_WARN("[LOGIN] User 0x%llX already logged in", (uintptr_t)&user);
		return;
	}

	LOG_INFO("[LOGIN] User 0x%llX, username: %s", (uintptr_t)&user, request.username().c_str());

	// 2. Player 생성
	Player* player = new Player(&user, request.username());

	// 3. User에 Player 설정
	user.SetPlayer(player);

	// 4. Scene 선택
	GameScene* scene = ChooseSceneForNewPlayer();

	// ══════════════════════════════════════════════════════
	// Scene Enter는 LogicThread로 위임
	// ══════════════════════════════════════════════════════
	scene->PostJob([player, scene]() {
		// LogicThread 컨텍스트
		scene->Enter(player);

		// 로그인 응답 전송
		User* owner = player->GetOwner();
		if (owner && owner->GetPlayer())
		{
			game::GC_LOGIN_RES response;
			response.set_success(true);
			response.set_player_id(player->GetPlayerId());

			auto* pos = response.mutable_spawn_pos();
			pos->set_x(0.0f);
			pos->set_y(0.0f);
			pos->set_z(0.0f);

			owner->SendPacket(response);
		}
	});
}

void GameServer::HandleMoveRequest(User& user, const game::CG_MOVE_REQ& request)
{
	// ══════════════════════════════════════════════════════
	// Worker Thread
	// ══════════════════════════════════════════════════════

	// 1. Player 조회
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

	// 2. Player의 Job Queue에 목표 위치 설정 Job 추가
	// cur_pos는 클라이언트가 보낸 현재 위치 (검증용, 현재는 사용 안 함)
	// move_pos가 실제 목표 위치
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

	// Player 조회
	Player* player = user->GetPlayer();
	if (player == nullptr)
	{
		// 로그인 안 한 상태로 disconnect
		LOG_INFO("[DISCONNECT] User 0x%llX disconnected (not logged in)", (uintptr_t)user);
		return;
	}

	LOG_INFO("[DISCONNECT] Player %s (User 0x%llX) disconnected",
		player->GetUsername().c_str(), (uintptr_t)user);

	// Scene Leave는 LogicThread로 위임
	GameScene* scene = player->GetScene();
	if (scene)
	{
		scene->PostJob([player, scene]() {
			// LogicThread 컨텍스트
			scene->Exit(player);

			// Player 삭제 (LogicThread에서 안전하게 삭제)
			delete player;
		});
	}
	else
	{
		// Scene에 들어가기 전에 disconnect한 경우
		delete player;
	}

	// User에서 Player 포인터 제거
	user->ClearPlayer();
}

void GameServer::OnServerStart()
{
	LOG_INFO("GameServer started successfully!");

	// GameScene 초기화
	scene_ = std::make_unique<GameScene>();

	// LogicThread 0번에 Scene 등록
	GetLogicThread(0)->AddScene(scene_.get());
}

void GameServer::OnServerStop()
{
	LOG_INFO("GameServer stopped successfully!");

	// GameScene 정리
	if (scene_)
	{
		GetLogicThread(0)->RemoveScene(scene_.get());
		scene_.reset();
	}
}

GameScene* GameServer::ChooseSceneForNewPlayer()
{
	// TODO: 여러 Scene 지원 시 확장
	// 현재는 단일 Scene만 지원
	return scene_.get();
}
