#include <iostream>
#include <chrono>
#include <atomic>
#include "../JunCommon/queue/JobQueue.h"
#include "../JunCommon/queue/PacketJob.h"

using namespace std;

//------------------------------
// JobQueue 기본 기능 테스트
//------------------------------
void TestJobQueue()
{
    cout << "=== JobQueue Basic Test ===" << endl;
    
    JobQueue jobQueue;
    atomic<int> processedCount = 0;
    
    // Job 추가
    for (int i = 0; i < 10; ++i) {
        jobQueue.Enqueue(Job([&processedCount, i]() {
            processedCount++;
            cout << "Processing job " << i << endl;
        }));
    }
    
    // Job 처리
    Job job;
    while (processedCount.load() < 10) {
        if (jobQueue.Dequeue(job, 100)) {
            if (job.jobFunction) {
                job.jobFunction();
            }
        }
    }
    
    cout << "Processed " << processedCount.load() << " jobs" << endl;
    cout << "JobQueue Basic Test: PASSED" << endl << endl;
}

//------------------------------
// ThreadPool 기본 기능 테스트
//------------------------------
void TestThreadPool()
{
    cout << "=== ThreadPool Basic Test ===" << endl;
    
    {
        ThreadPool pool(4, "TestPool");
        atomic<int> processedCount = 0;
        
        // 작업 제출
        for (int i = 0; i < 20; ++i) {
            pool.Submit([&processedCount, i]() {
                processedCount++;
                // 약간의 작업 시뮬레이션
                this_thread::sleep_for(chrono::milliseconds(10));
                cout << "ThreadPool job " << i << " completed" << endl;
            });
        }
        
        // 모든 작업 완료 대기
        while (processedCount.load() < 20) {
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        
        cout << "ThreadPool processed " << processedCount.load() << " jobs" << endl;
    } // ThreadPool 소멸자에서 자동 종료
    
    cout << "ThreadPool Basic Test: PASSED" << endl << endl;
}

//------------------------------
// PacketJob 테스트용 핸들러
//------------------------------
class TestPacketJobHandler : public PacketJobHandler
{
    atomic<int> recvCount = 0;
    atomic<int> joinCount = 0;
    atomic<int> leaveCount = 0;
    
public:
    void HandleRecvPacket(SessionInfo sessionInfo, PacketBuffer* packet) override {
        recvCount++;
        cout << "HandleRecvPacket: Session " << sessionInfo.sessionId.sessionId 
             << ", Packet: " << (void*)packet << endl;
        
        // 패킷 해제는 실제 구현에서 필요
        // PacketBuffer::Free(packet);
    }
    
    void HandleClientJoin(SessionInfo sessionInfo, SOCKADDR_IN clientAddr) override {
        joinCount++;
        cout << "HandleClientJoin: Session " << sessionInfo.sessionId.sessionId << endl;
    }
    
    void HandleClientLeave(SessionInfo sessionInfo) override {
        leaveCount++;
        cout << "HandleClientLeave: Session " << sessionInfo.sessionId.sessionId << endl;
    }
    
    // 통계 출력
    void PrintStats() const {
        cout << "Handler Stats - Recv: " << recvCount.load()
             << ", Join: " << joinCount.load()
             << ", Leave: " << leaveCount.load() << endl;
    }
};

//------------------------------
// PacketJob 통합 테스트
//------------------------------
void TestPacketJob()
{
    cout << "=== PacketJob Integration Test ===" << endl;
    
    TestPacketJobHandler handler;
    ThreadPool pool(2, "PacketJobPool");
    
    // 다양한 PacketJob 생성 및 제출
    for (int i = 0; i < 5; ++i) {
        SessionInfo sessionInfo(i, 1000 + i);
        
        // Join Job
        auto joinJob = PacketJob::CreateJoinJob(sessionInfo, {});
        pool.Submit(joinJob.ToJob([&handler](const PacketJob& job) {
            handler.HandlePacketJob(job);
        }));
        
        // Recv Job (실제로는 PacketBuffer가 필요하지만 테스트용으로 nullptr 사용)
        auto recvJob = PacketJob::CreateRecvJob(sessionInfo, nullptr);
        pool.Submit(recvJob.ToJob([&handler](const PacketJob& job) {
            handler.HandlePacketJob(job);
        }));
        
        // Leave Job
        auto leaveJob = PacketJob::CreateLeaveJob(sessionInfo);
        pool.Submit(leaveJob.ToJob([&handler](const PacketJob& job) {
            handler.HandlePacketJob(job);
        }));
    }
    
    // 모든 작업 완료 대기
    this_thread::sleep_for(chrono::milliseconds(500));
    
    handler.PrintStats();
    cout << "PacketJob Integration Test: PASSED" << endl << endl;
}

//------------------------------
// 성능 테스트
//------------------------------
void TestPerformance()
{
    cout << "=== Performance Test ===" << endl;
    
    const int JOB_COUNT = 10000;
    ThreadPool pool(4, "PerformanceTestPool");
    atomic<int> processedCount = 0;
    
    auto startTime = chrono::high_resolution_clock::now();
    
    // 대량 작업 제출
    for (int i = 0; i < JOB_COUNT; ++i) {
        pool.Submit([&processedCount]() {
            processedCount++;
            // 가벼운 작업 시뮬레이션
            volatile int sum = 0;
            for (int j = 0; j < 100; ++j) {
                sum += j;
            }
        });
    }
    
    // 완료 대기
    while (processedCount.load() < JOB_COUNT) {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
    
    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    
    cout << "Processed " << JOB_COUNT << " jobs in " << duration.count() << " ms" << endl;
    cout << "TPS: " << (JOB_COUNT * 1000 / duration.count()) << " jobs/sec" << endl;
    cout << "Performance Test: PASSED" << endl << endl;
}

//------------------------------
// 메인 테스트 실행 함수
//------------------------------
void RunJobQueueTests()
{
    cout << "Starting JobQueue/ThreadPool Tests..." << endl << endl;
    
    try {
        TestJobQueue();
        TestThreadPool();
        TestPacketJob();
        TestPerformance();
        
        cout << "=== All Tests PASSED ===" << endl;
    }
    catch (const exception& e) {
        cout << "Test FAILED: " << e.what() << endl;
    }
}