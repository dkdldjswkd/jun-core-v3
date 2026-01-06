#include "LogicThread.h"
#include "JobObject.h"
#include "GameScene.h"

LogicThread::LogicThread()
{
}

LogicThread::~LogicThread()
{
    Stop();
}

void LogicThread::AddScene(GameScene* scene)
{
    m_scenes.push_back(scene);
}

void LogicThread::RemoveScene(GameScene* scene)
{
    auto it = std::find(m_scenes.begin(), m_scenes.end(), scene);
    if (it != m_scenes.end())
    {
        m_scenes.erase(it);
    }
}

void LogicThread::Start()
{
    m_running.store(true);
    m_lastFrameTime = std::chrono::steady_clock::now();

    m_worker = std::thread([this]() {
        Run();
    });
}

void LogicThread::Stop()
{
    m_running.store(false);

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void LogicThread::Run()
{
    // TLS 초기화
    Time::SetDeltaTime(0.0f);
    Time::SetFixedDeltaTime(m_fixedTimeStep);
    Time::SetTime(0.0f);
    Time::SetFrameCount(0);

    while (m_running.load())
    {
        float dt = CalcDeltaTime();

        // Time 갱신 (TLS)
        Time::SetDeltaTime(dt);
        Time::SetTime(Time::time() + dt);
        Time::SetFrameCount(Time::frameCount() + 1);
        Time::SetFixedDeltaTime(m_fixedTimeStep);

        m_fixedTimeAccum += dt;

        // ──────── 1. Job 처리 ────────
        JobObject* jobObj = nullptr;
        while (m_jobQueue.Dequeue(&jobObj))
        {
            jobObj->Flush();
        }

        // ──────── 2. FixedUpdate (고정 간격) ────────
        while (m_fixedTimeAccum >= m_fixedTimeStep)
        {
            for (auto* scene : m_scenes)
            {
                scene->FixedUpdate();
            }

            m_fixedTimeAccum -= m_fixedTimeStep;
        }

        // ──────── 3. Update (프레임당 1회) ────────
        for (auto* scene : m_scenes)
        {
            scene->Update();
        }

        // ──────── 4. 프레임 대기 ────────
        // 60 FPS 목표 (16.66ms)
        const float TARGET_FRAME_TIME = 0.01666f;
        float sleepTime = TARGET_FRAME_TIME - dt;

        if (sleepTime > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(sleepTime)
            );
        }
    }
}

float LogicThread::CalcDeltaTime()
{
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    return elapsed.count();
}
