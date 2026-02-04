#pragma once
#include "JobThread.h"
#include "Time.h"
#include <vector>
#include <chrono>

class GameScene;

//------------------------------
// GameThread - Unity 스타일 게임 로직 스레드
// JobThread 상속 + Scene 관리 + FixedUpdate/Update
//------------------------------
class GameThread : public JobThread
{
private:
    std::vector<GameScene*> m_scenes;

    float m_fixedTimeAccum = 0.0f;
    float m_fixedTimeStep = 0.02f;  // 50Hz (기본값)

    std::chrono::steady_clock::time_point m_lastFrameTime;

public:
    GameThread();
    virtual ~GameThread() override;

    //------------------------------
    // Scene 관리
    //------------------------------
    void AddScene(GameScene* scene);
    void RemoveScene(GameScene* scene);

    //------------------------------
    // 스레드 시작/종료 (override)
    //------------------------------
    void Start() override;
    void Stop() override;

    //------------------------------
    // FixedUpdate 간격 설정
    //------------------------------
    void SetFixedTimeStep(float timeStep) { m_fixedTimeStep = timeStep; }
    float GetFixedTimeStep() const { return m_fixedTimeStep; }

protected:
    //------------------------------
    // 메인 루프 (override)
    //------------------------------
    void Run() override;

private:
    //------------------------------
    // 시간 계산
    //------------------------------
    float CalcDeltaTime();
};
