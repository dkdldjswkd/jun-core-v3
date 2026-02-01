#pragma once
#include "Entity.h"
#include "JobObject.h"
#include "../core/Event.h"
#include <cstdint>

class GameScene;
class GameObjectManager;

//------------------------------
// GameObject - Unity 스타일 게임 객체
// Player, Monster, NPC 등의 기반 클래스
//
// Entity 상속:
// - 컴포넌트 시스템 (AddComponent, GetComponent)
//
// JobObject 상속:
// - GameObject는 Scene에 종속되어 존재
// - Scene 이동 시 LogicThread도 함께 이동 가능
// - 모든 상태 변경을 Job으로 처리하여 스레드 안전 보장
//------------------------------
class GameObject : public Entity, public JobObject
{
protected:
    GameScene* m_pScene = nullptr;
    uint64_t m_sn = 0;  // Serial Number (GameObjectManager에서 부여)

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

    friend class GameScene;
    friend class GameObjectManager;

    //------------------------------
    // 생성자 (protected - Factory 패턴 사용)
    //------------------------------
    explicit GameObject(GameScene* scene);

public:
    virtual ~GameObject();

    //------------------------------
    // Event: 삭제 전 알림
    //------------------------------
    Event<> OnBeforeDestroy;

    //------------------------------
    // 삭제 처리 (OnBeforeDestroy 발행 후 MarkForDelete)
    //------------------------------
    void Destroy();

    //------------------------------
    // Scene 관리
    //------------------------------
    void MoveToScene(GameScene* newScene);
    void ExitScene();

    //------------------------------
    // Scene 접근
    //------------------------------
    GameScene* GetScene() { return m_pScene; }
    bool IsInScene() const { return m_pScene != nullptr; }

    //------------------------------
    // Serial Number 접근
    //------------------------------
    uint64_t GetSN() const { return m_sn; }
};
