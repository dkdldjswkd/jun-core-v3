#pragma once
#include "../../JunCommon/container/LFQueue.h"
#include <thread>
#include <atomic>
#include <functional>

using Job = std::function<void()>;

class JobObject;

//------------------------------
// JobThread - JobObject 처리 스레드 기반 클래스
// LogicThread가 상속하여 Scene Update 기능 추가
// GameObjectManager 등 시스템 매니저들은 JobObject로 이 스레드 공유
//------------------------------
class JobThread
{
protected:
    LFQueue<JobObject*> m_jobObjectQueue;

    std::thread m_worker;
    std::atomic<bool> m_running{false};

public:
    JobThread() = default;
    virtual ~JobThread();

    // 복사/이동 금지
    JobThread(const JobThread&) = delete;
    JobThread& operator=(const JobThread&) = delete;

    //------------------------------
    // JobObject 큐 접근 (JobObject::PostJob에서 사용)
    //------------------------------
    LFQueue<JobObject*>* GetJobQueue() { return &m_jobObjectQueue; }

    //------------------------------
    // 스레드 시작/종료
    //------------------------------
    virtual void Start();
    virtual void Stop();

    //------------------------------
    // 상태 확인
    //------------------------------
    bool IsRunning() const { return m_running.load(); }

protected:
    //------------------------------
    // 메인 루프 (virtual - 서브클래스에서 확장)
    //------------------------------
    virtual void Run();

    //------------------------------
    // JobObject 처리 (LogicThread에서도 호출)
    //------------------------------
    void ProcessJobObjects();
};
