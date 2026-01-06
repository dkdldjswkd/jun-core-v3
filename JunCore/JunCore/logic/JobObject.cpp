#include "JobObject.h"
#include "LogicThread.h"
#include <stdexcept>

JobObject::JobObject(LogicThread* logicThread)
    : m_pLogicThread(logicThread)
{
    if (logicThread == nullptr)
    {
        throw std::invalid_argument("JobObject: LogicThread cannot be null");
    }
}

JobObject::~JobObject()
{
    // 남은 Job 정리
    Job job;
    while (m_jobQueue.Dequeue(&job))
    {
        // 소멸자에서는 실행하지 않고 버림
    }
}

void JobObject::PostJob(Job job)
{
    m_jobQueue.Enqueue(std::move(job));

    // CAS로 스케줄 시도
    bool expected = false;
    if (m_processing.compare_exchange_strong(expected, true))
    {
        m_pLogicThread->GetJobQueue()->Enqueue(this);
    }
}

void JobObject::Flush()
{
    LogicThread* pOldThread = m_pLogicThread;  // 현재 스레드 저장

    Job job;
    while (m_jobQueue.Dequeue(&job))
    {
        job();  // Job 실행

        // 스레드가 변경되었다면 (ChangeScene 등)
        if (m_pLogicThread != pOldThread)
        {
            // 새 스레드에 등록하고 즉시 종료
            // m_processing은 true 유지 (아직 처리 중)
            m_pLogicThread->GetJobQueue()->Enqueue(this);
            return;
        }
    }

    // 처리 완료
    m_processing.store(false);

    // Lost Wakeup 방지: Flush 완료 직후 새로운 Job이 들어왔는지 체크
    // 스레드 변경되지 않은 경우에만 재스케줄
    if (m_jobQueue.GetUseCount() > 0)
    {
        bool expected = false;
        if (m_processing.compare_exchange_strong(expected, true))
        {
            m_pLogicThread->GetJobQueue()->Enqueue(this);
        }
    }
}
