#pragma once
#include "../../JunCommon/container/LFQueue.h"
#include <vector>
#include <algorithm>
#include <functional>

class GameThread;
class GameObject;

//------------------------------
// GameScene - Unity 스타일 씬
// Map, Dungeon, Lobby 등의 기반 클래스
//------------------------------
class GameScene
{
protected:
    std::vector<GameObject*> m_objects;
    GameThread* m_pGameThread = nullptr;

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

public:
    explicit GameScene(GameThread* gameThread);
    virtual ~GameScene();

    //------------------------------
    // GameObject 관리
    //------------------------------
    void Enter(GameObject* obj);
    void Exit(GameObject* obj);

    //------------------------------
    // 프레임 업데이트 (GameThread에서 호출)
    //------------------------------
    void FixedUpdate();
    void Update();

    //------------------------------
    // GameThread 관리
    //------------------------------
    GameThread* GetGameThread() { return m_pGameThread; }

    //------------------------------
    // GameObject 조회
    //------------------------------
    const std::vector<GameObject*>& GetObjects() const { return m_objects; }

};
