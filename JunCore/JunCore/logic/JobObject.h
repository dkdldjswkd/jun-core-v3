#pragma once
#include "../../JunCommon/container/LFQueue.h"
#include <functional>
#include <atomic>

class JobThread;

//------------------------------
// Job - 처리할 작업 단위
//------------------------------
using Job = std::function<void()>;

//------------------------------
// JobObject - Job을 받아서 처리하는 객체
// Player, Monster, GameObjectManager 등이 상속
// JobThread (또는 GameThread)에서 Flush됨
//------------------------------
class JobObject
{
protected:
    LFQueue<Job> m_jobQueue;
    std::atomic<bool> m_processing{false};
    std::atomic<bool> m_markedForDelete{false};
    JobThread* m_pJobThread;

public:
    // 기본 생성자 (싱글톤 패턴용 - Initialize에서 JobThread 설정 필수)
    JobObject();
    explicit JobObject(JobThread* jobThread);
    virtual ~JobObject();

    //------------------------------
    // Job 추가 (네트워크 스레드에서 호출)
    // 삭제 마킹된 경우 false 반환
    //------------------------------
    bool PostJob(Job job);

    //------------------------------
    // Job 처리 (JobThread에서 호출)
    // Lost Wakeup 방지 로직 포함
    //------------------------------
    void Flush();

    //------------------------------
    // 삭제 마킹 (이후 PostJob 거부됨)
    //------------------------------
    void MarkForDelete();
    bool IsMarkedForDelete() const { return m_markedForDelete.load(); }

    //------------------------------
    // JobThread 관리
    //------------------------------
    JobThread* GetJobThread() { return m_pJobThread; }
    void SetJobThread(JobThread* thread);
};
