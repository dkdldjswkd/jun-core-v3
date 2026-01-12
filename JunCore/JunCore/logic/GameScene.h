#pragma once
#include "../../JunCommon/container/LFQueue.h"
#include <vector>
#include <algorithm>
#include <functional>

class LogicThread;
class GameObject;

//------------------------------
// GameScene - Unity 스타일 씬
// Map, Dungeon, Lobby 등의 기반 클래스
//------------------------------
class GameScene
{
protected:
    std::vector<GameObject*> m_objects;
    LogicThread* m_pLogicThread = nullptr;

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

public:
    explicit GameScene(LogicThread* logicThread);
    virtual ~GameScene();

    //------------------------------
    // GameObject 관리
    //------------------------------
    void Enter(GameObject* obj);
    void Exit(GameObject* obj);

    //------------------------------
    // 프레임 업데이트 (LogicThread에서 호출)
    //------------------------------
    void FixedUpdate();
    void Update();

    //------------------------------
    // LogicThread 관리
    //------------------------------
    LogicThread* GetLogicThread() { return m_pLogicThread; }

    //------------------------------
    // GameObject 조회
    //------------------------------
    const std::vector<GameObject*>& GetObjects() const { return m_objects; }

};
