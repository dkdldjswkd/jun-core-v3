#pragma once
#include "Entity.h"
#include "JobObject.h"
#include "../core/Event.h"
#include <cstdint>
#include <vector>

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
// - Scene 이동 시 GameThread도 함께 이동 가능
// - 모든 상태 변경을 Job으로 처리하여 스레드 안전 보장
//------------------------------
class GameObject : public Entity, public JobObject
{
protected:
    GameScene* m_pScene = nullptr;
    uint64_t m_sn = 0;

    // 월드 좌표 (AoiGrid와 연동)
    float m_x{0.f};
    float m_y{0.f};
    float m_z{0.f};

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}


    friend class GameScene;

    //------------------------------
    // 생성자 (protected)
    //------------------------------
    explicit GameObject(GameScene* scene, float x, float y, float z);

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

    //------------------------------
    // AOI 이벤트 (AoiGrid에서 호출, public)
    // 상대방이 내 시야에 들어옴/나감
    //------------------------------
    virtual void OnAppear(std::vector<GameObject*>& others) {}
    virtual void OnDisappear(std::vector<GameObject*>& others) {}

    //------------------------------
    // 위치 관리 (AoiGrid 연동)
    //------------------------------
    void SetPosition(float x, float y, float z);

    float GetX() const { return m_x; }
    float GetY() const { return m_y; }
    float GetZ() const { return m_z; }
};
