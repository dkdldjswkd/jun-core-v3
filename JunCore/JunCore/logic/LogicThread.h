#pragma once
#include "JobObject.h"
#include "Time.h"
#include "../../JunCommon/container/LFQueue.h"
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

class GameScene;

//------------------------------
// LogicThread - Unity 스타일 게임 로직 스레드
// Job 처리 + FixedUpdate + Update 실행
//------------------------------
class LogicThread
{
private:
    std::vector<GameScene*> m_scenes;
    LFQueue<JobObject*> m_jobQueue;

    std::thread m_worker;
    std::atomic<bool> m_running{false};

    float m_fixedTimeAccum = 0.0f;
    float m_fixedTimeStep = 0.02f;  // 50Hz (기본값)

    std::chrono::steady_clock::time_point m_lastFrameTime;

public:
    LogicThread();
    ~LogicThread();

    //------------------------------
    // Scene 관리
    //------------------------------
    void AddScene(GameScene* scene);
    void RemoveScene(GameScene* scene);

    //------------------------------
    // JobQueue 접근
    //------------------------------
    LFQueue<JobObject*>* GetJobQueue() { return &m_jobQueue; }

    //------------------------------
    // 스레드 시작/종료
    //------------------------------
    void Start();
    void Stop();

    //------------------------------
    // FixedUpdate 간격 설정
    //------------------------------
    void SetFixedTimeStep(float timeStep) { m_fixedTimeStep = timeStep; }
    float GetFixedTimeStep() const { return m_fixedTimeStep; }

private:
    //------------------------------
    // 메인 루프
    //------------------------------
    void Run();

    //------------------------------
    // 시간 계산
    //------------------------------
    float CalcDeltaTime();
};
