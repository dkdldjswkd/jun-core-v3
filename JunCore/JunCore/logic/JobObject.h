#pragma once
#include "../../JunCommon/container/LFQueue.h"
#include <functional>
#include <atomic>

class LogicThread;

//------------------------------
// Job - 처리할 작업 단위
//------------------------------
using Job = std::function<void()>;

//------------------------------
// JobObject - Job을 받아서 처리하는 객체
// Player, Monster 등이 상속
//------------------------------
class JobObject
{
protected:
    LFQueue<Job> m_jobQueue;
    std::atomic<bool> m_processing{false};
    LogicThread* m_pLogicThread;

public:
    JobObject() = delete;
    explicit JobObject(LogicThread* logicThread);
    virtual ~JobObject();

    //------------------------------
    // Job 추가 (네트워크 스레드에서 호출)
    //------------------------------
    void PostJob(Job job);

    //------------------------------
    // Job 처리 (LogicThread에서 호출)
    // Lost Wakeup 방지 로직 포함
    //------------------------------
    void Flush();

    //------------------------------
    // LogicThread 관리
    //------------------------------
    LogicThread* GetLogicThread() { return m_pLogicThread; }
    void SetLogicThread(LogicThread* thread)
    {
        if (thread == nullptr)
        {
            throw std::invalid_argument("JobObject::SetLogicThread: LogicThread cannot be null");
        }
        m_pLogicThread = thread;
    }
};
