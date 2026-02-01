#include "JobObject.h"
#include "JobThread.h"
#include <stdexcept>

JobObject::JobObject()
    : m_pJobThread(nullptr)
{
    // 기본 생성자 - Initialize에서 JobThread 설정 필수
}

JobObject::JobObject(JobThread* jobThread)
    : m_pJobThread(jobThread)
{
    if (jobThread == nullptr)
    {
        throw std::invalid_argument("JobObject: JobThread cannot be null");
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

bool JobObject::PostJob(Job job)
{
    // 삭제 마킹된 경우 Job 거부
    if (m_markedForDelete.load())
    {
        return false;
    }

    m_jobQueue.Enqueue(std::move(job));

    // CAS로 스케줄 시도
    bool expected = false;
    if (m_processing.compare_exchange_strong(expected, true))
    {
        m_pJobThread->GetJobQueue()->Enqueue(this);
    }

    return true;
}

void JobObject::MarkForDelete()
{
    m_markedForDelete.store(true);
}

void JobObject::SetJobThread(JobThread* thread)
{
    if (thread == nullptr)
    {
        throw std::invalid_argument("JobObject::SetJobThread: JobThread cannot be null");
    }
    m_pJobThread = thread;
}

void JobObject::Flush()
{
	if (m_markedForDelete.load())
	{
		return;
	}

    JobThread* pOldThread = m_pJobThread;

    Job job;
    while (m_jobQueue.Dequeue(&job))
    {
		job();

        if (m_markedForDelete.load())
        {
            return;
        }

        // 스레드가 변경되었다면
        if (m_pJobThread != pOldThread)
        {
            // 새 스레드에 등록하고 종료
            m_pJobThread->GetJobQueue()->Enqueue(this);
            return;
        }
    }

    m_processing.store(false);

    // Lost Wakeup 방지
    if (m_jobQueue.GetUseCount() > 0)
    {
        bool expected = false;
        if (m_processing.compare_exchange_strong(expected, true))
        {
            m_pJobThread->GetJobQueue()->Enqueue(this);
        }
    }
}
