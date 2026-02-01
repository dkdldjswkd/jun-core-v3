#include "JobThread.h"
#include "JobObject.h"
#include <chrono>

JobThread::~JobThread()
{
    Stop();
}

void JobThread::Start()
{
    if (m_running.load())
    {
        return;
    }

    m_running.store(true);
    m_worker = std::thread([this]() {
        Run();
    });
}

void JobThread::Stop()
{
    m_running.store(false);

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void JobThread::Run()
{
    while (m_running.load())
    {
        ProcessJobObjects();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // 종료 전 남은 JobObject 모두 처리
    ProcessJobObjects();
}

void JobThread::ProcessJobObjects()
{
    JobObject* jobObj = nullptr;
    while (m_jobObjectQueue.Dequeue(&jobObj))
    {
        jobObj->Flush();

        if (jobObj->IsMarkedForDelete())
        {
            delete jobObj;
        }
    }
}
