#pragma once
#include "GameObject.h"
#include "../../JunCommon/container/LFQueue.h"
#include <vector>
#include <algorithm>
#include <functional>

class LogicThread;

//------------------------------
// GameScene - Unity 스타일 씬
// Map, Dungeon, Lobby 등의 기반 클래스
//------------------------------
class GameScene
{
protected:
    std::vector<GameObject*> m_objects;
    LogicThread* m_pLogicThread = nullptr;
    LFQueue<std::function<void()>> m_jobQueue;  // Scene 전용 Job 큐

    //------------------------------
    // 가상 함수 (사용자 구현)
    //------------------------------
    virtual void OnFixedUpdate() {}
    virtual void OnUpdate() {}

public:
    GameScene() = default;
    virtual ~GameScene() = default;

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
    void SetLogicThread(LogicThread* thread) { m_pLogicThread = thread; }

    //------------------------------
    // GameObject 조회
    //------------------------------
    const std::vector<GameObject*>& GetObjects() const { return m_objects; }

    //------------------------------
    // Job 편의 메서드 (LogicThread로 위임)
    //------------------------------
    template<typename Func>
    void PostJob(Func&& func)
    {
        m_jobQueue.Enqueue(std::forward<Func>(func));
    }

    //------------------------------
    // Job 처리 (LogicThread에서 호출)
    //------------------------------
    void FlushJobs();
};
