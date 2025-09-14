#pragma once
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

//------------------------------
// OnceInitializer 대기 정책들
// 정책 패턴으로 다양한 대기 전략 제공
//------------------------------

//------------------------------
// 스핀 대기 정책 (기본값) - 빠른 초기화용
//------------------------------
struct SpinWaitPolicy
{
    static void Wait() noexcept
    {
        std::this_thread::yield();
    }
};

//------------------------------
// 뮤텍스 대기 정책 - 긴 초기화용
//------------------------------
class MutexWaitPolicy
{
private:
    static std::mutex mutex_;
    static std::condition_variable cv_;
    static bool notified_;

public:
    static void Wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!notified_) {
            cv_.wait(lock, [] { return notified_; });
        }
    }
    
    static void NotifyCompletion()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            notified_ = true;
        }
        cv_.notify_all();
    }
    
    static void Reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        notified_ = false;
    }
};

//------------------------------
// 하이브리드 대기 정책 - 자동 적응
//------------------------------
struct HybridWaitPolicy
{
    static void Wait() noexcept
    {
        static thread_local int spin_count = 0;
        
        if (spin_count++ < 1000) {
            // 처음엔 스핀
            std::this_thread::yield();
        } else {
            // 오래 걸리면 슬립
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
};

//------------------------------
// 정적 멤버 정의
//------------------------------
inline std::mutex MutexWaitPolicy::mutex_;
inline std::condition_variable MutexWaitPolicy::cv_;
inline bool MutexWaitPolicy::notified_{false};