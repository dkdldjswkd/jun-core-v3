#pragma once
#include <Windows.h>
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include "../container/LFQueue.h"

//------------------------------
// Job - 패킷 처리 작업 단위
//------------------------------
struct Job
{
    using JobFunction = std::function<void()>;
    
    JobFunction jobFunction;
    DWORD priority = 0;  // 0 = highest priority
    
    Job() = default;
    Job(JobFunction&& func, DWORD prio = 0) 
        : jobFunction(std::move(func)), priority(prio) {}
};

//------------------------------
// JobQueue - Lock-free Job 큐
//------------------------------
class JobQueue
{
public:
    JobQueue();
    ~JobQueue();
    
    // Job 추가 (Producer)
    void Enqueue(Job&& job);
    void Enqueue(const Job& job);
    
    // Job 처리 (Consumer) - 블로킹
    bool Dequeue(Job& outJob, DWORD timeoutMs = INFINITE);
    
    // 통계
    size_t GetPendingCount() const { return jobQueue.GetUseCount(); }
    
    // 종료 신호
    void Shutdown();
    bool IsShutdown() const { return isShutdown.load(); }

private:
    LFQueue<Job> jobQueue;
    HANDLE semaphore;
    std::atomic<bool> isShutdown;
};

//------------------------------
// ThreadPool - Job 처리 스레드 풀
//------------------------------
class ThreadPool
{
public:
    ThreadPool(size_t threadCount, const char* poolName = "ThreadPool");
    ~ThreadPool();
    
    // Job 제출
    template<typename Func, typename... Args>
    void Submit(Func&& func, Args&&... args);
    
    void Submit(Job&& job);
    void Submit(const Job& job);
    
    // 통계 및 모니터링
    size_t GetPendingJobs() const { return jobQueue.GetPendingCount(); }
    size_t GetThreadCount() const { return workerThreads.size(); }
    DWORD GetProcessedJobCount() const { return processedJobCount.load(); }
    
    // 종료
    void Shutdown();
    bool IsShutdown() const { return isShutdown.load(); }

private:
    void WorkerThreadFunc();
    
private:
    JobQueue jobQueue;
    std::vector<std::thread> workerThreads;
    std::atomic<bool> isShutdown;
    std::atomic<DWORD> processedJobCount;
    std::string poolName;
};

//------------------------------
// JobQueue 인라인 구현
//------------------------------

inline JobQueue::JobQueue() 
    : semaphore(CreateSemaphore(NULL, 0, MAXLONG, NULL))
    , isShutdown(false)
{
    if (semaphore == NULL) {
        throw std::exception("Failed to create semaphore for JobQueue");
    }
}

inline JobQueue::~JobQueue()
{
    Shutdown();
    if (semaphore != NULL) {
        CloseHandle(semaphore);
    }
}

inline void JobQueue::Enqueue(Job&& job)
{
    if (isShutdown.load()) return;
    
    jobQueue.Enqueue(std::move(job));
    ReleaseSemaphore(semaphore, 1, NULL);
}

inline void JobQueue::Enqueue(const Job& job)
{
    if (isShutdown.load()) return;
    
    jobQueue.Enqueue(job);
    ReleaseSemaphore(semaphore, 1, NULL);
}

inline bool JobQueue::Dequeue(Job& outJob, DWORD timeoutMs)
{
    if (isShutdown.load()) return false;
    
    DWORD waitResult = WaitForSingleObject(semaphore, timeoutMs);
    if (waitResult != WAIT_OBJECT_0) {
        return false;  // Timeout or error
    }
    
    if (isShutdown.load()) return false;
    
    return jobQueue.Dequeue(&outJob);
}

inline void JobQueue::Shutdown()
{
    isShutdown.store(true);
    
    // 모든 대기중인 스레드를 깨우기 위해 세마포어 해제
    for (int i = 0; i < 100; ++i) {  // 충분한 수만큼 신호
        ReleaseSemaphore(semaphore, 1, NULL);
    }
}

//------------------------------
// ThreadPool 인라인 구현
//------------------------------

inline ThreadPool::ThreadPool(size_t threadCount, const char* poolName)
    : isShutdown(false)
    , processedJobCount(0)
    , poolName(poolName ? poolName : "ThreadPool")
{
    workerThreads.reserve(threadCount);
    
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads.emplace_back([this]() {
            WorkerThreadFunc();
        });
    }
}

inline ThreadPool::~ThreadPool()
{
    Shutdown();
}

template<typename Func, typename... Args>
inline void ThreadPool::Submit(Func&& func, Args&&... args)
{
    if (isShutdown.load()) return;
    
    auto boundFunc = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    Job job(std::move(boundFunc));
    jobQueue.Enqueue(std::move(job));
}

inline void ThreadPool::Submit(Job&& job)
{
    if (isShutdown.load()) return;
    jobQueue.Enqueue(std::move(job));
}

inline void ThreadPool::Submit(const Job& job)
{
    if (isShutdown.load()) return;
    jobQueue.Enqueue(job);
}

inline void ThreadPool::Shutdown()
{
    if (isShutdown.exchange(true)) return;  // 이미 종료 중
    
    jobQueue.Shutdown();
    
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    workerThreads.clear();
}

inline void ThreadPool::WorkerThreadFunc()
{
    Job job;
    
    while (!isShutdown.load()) {
        if (jobQueue.Dequeue(job, 100)) {  // 100ms timeout
            try {
                if (job.jobFunction) {
                    job.jobFunction();
                    processedJobCount.fetch_add(1);
                }
            }
            catch (...) {
                // Job 실행 중 예외 발생 - 로그 출력하고 계속 진행
                // 향후 Logger 통합 시 로깅 추가 예정
            }
        }
    }
}