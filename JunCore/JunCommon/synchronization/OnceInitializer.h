#pragma once
#include <atomic>
#include <functional>
#include <thread>
#include "OnceInitializerPolicies.h"

//------------------------------
// OnceInitializer - 범용 "한 번만 초기화" 템플릿 클래스
// Reference counting + 정책 패턴으로 다양한 대기 전략 지원
//
// 사용 예시:
//   // 기본 (스핀락)
//   using WSAInit = OnceInitializer<>;
//   
//   // 뮤텍스 방식 (긴 초기화용)
//   using DBInit = OnceInitializer<std::function<bool()>, std::function<void()>, MutexWaitPolicy>;
//   
//   // 하이브리드 방식
//   using FileInit = OnceInitializer<std::function<bool()>, std::function<void()>, HybridWaitPolicy>;
//------------------------------
template<typename InitFunc = std::function<bool()>, typename CleanupFunc = std::function<void()>, typename WaitPolicy = SpinWaitPolicy>
class OnceInitializer
{
public:
    //------------------------------
    // 초기화 상태 열거형
    //------------------------------
    enum InitState : int {
        NOT_INITIALIZED = 0,    // 초기화 안됨
        INITIALIZING = 1,       // 초기화 중
        INITIALIZED = 2,        // 초기화 완료 
        FAILED = -1             // 초기화 실패
    };
    
    //------------------------------
    // 초기화 - 첫 번째 호출에서만 init_func 실행
    // Race Condition 완전 해결: 초기화 완료까지 대기
    //------------------------------
    static bool Initialize(InitFunc init_func)
    {
        reference_count_++;
        
        // 현재 상태 확인
        int current_state = state_.load();
        
        // 이미 완료된 경우
        if (current_state == INITIALIZED) {
            return true;
        }
        
        // 이미 실패한 경우  
        if (current_state == FAILED) {
            reference_count_--;
            return false;
        }
        
        // 첫 번째 초기화 시도
        int expected = NOT_INITIALIZED;
        if (state_.compare_exchange_strong(expected, INITIALIZING))
        {
            // 오직 한 스레드만 여기 진입 - 실제 초기화 수행
            try 
            {
                if (init_func()) 
                {
                    state_ = INITIALIZED;  // 성공
                    NotifyWaiters();       // 대기 중인 스레드들에게 알림
                    return true;
                } 
                else 
                {
                    state_ = FAILED;       // 실패
                    NotifyWaiters();       // 실패도 알림
                    reference_count_--;
                    return false;
                }
            } 
            catch (...) 
            {
                state_ = FAILED;           // 예외 시 실패
                NotifyWaiters();           // 예외도 알림
                reference_count_--;
                throw;
            }
        }
        
        // 다른 스레드가 초기화 중 - 정책에 따른 대기
        while (true) 
        {
            current_state = state_.load();
            
            if (current_state == INITIALIZED) 
            {
                return true;
            }
            
            if (current_state == FAILED) 
            {
                reference_count_--;
                return false;
            }
            
            // INITIALIZING 상태 - 정책에 따른 대기
            WaitPolicy::Wait();
        }
    }
    
    //------------------------------
    // 정리 - 마지막 참조에서만 cleanup_func 실행
    //------------------------------
    static void Cleanup(CleanupFunc cleanup_func)
    {
        if (reference_count_.fetch_sub(1) == 1) 
        {
            // 마지막 참조였음 - 정리 실행
            try {
                cleanup_func();
            } catch (...) {
                // 예외는 무시 (소멸자에서 호출될 가능성)
            }
            state_ = NOT_INITIALIZED;  // 상태 리셋
        }
    }
    
    //------------------------------
    // 상태 확인
    //------------------------------
    static bool IsInitialized() noexcept
    {
        return state_.load() == INITIALIZED;
    }
    
    //------------------------------
    // 상태 값 확인 (디버그용)
    //------------------------------
    static int GetState() noexcept
    {
        return state_.load();
    }
    
    //------------------------------
    // 현재 참조 개수 확인 (디버그용)
    //------------------------------
    static int GetReferenceCount() noexcept
    {
        return reference_count_.load();
    }
    
    //------------------------------
    // 강제 리셋 (테스트용 - 운영에서는 사용 금지!)
    //------------------------------
    static void ForceReset() noexcept
    {
        state_ = NOT_INITIALIZED;
        reference_count_ = 0;
        ResetWaitPolicy();
    }

private:
    //------------------------------
    // 정책별 알림 처리 (SFINAE 활용)
    //------------------------------
    template<typename T = WaitPolicy>
    static auto NotifyWaiters() -> decltype(T::NotifyCompletion(), void())
    {
        T::NotifyCompletion();
    }
    
    template<typename T = WaitPolicy>
    static auto NotifyWaiters(...) -> void
    {
        // SpinWaitPolicy 등 알림이 없는 정책용 - 아무것도 안함
    }
    
    template<typename T = WaitPolicy>
    static auto ResetWaitPolicy() -> decltype(T::Reset(), void())
    {
        T::Reset();
    }
    
    template<typename T = WaitPolicy>
    static auto ResetWaitPolicy(...) -> void
    {
        // SpinWaitPolicy 등 리셋이 없는 정책용 - 아무것도 안함
    }
    
    //------------------------------
    // 정적 멤버 변수
    //------------------------------
    static std::atomic<int> state_;
    static std::atomic<int> reference_count_;
};

//------------------------------
// 정적 멤버 정의
//------------------------------
template<typename InitFunc, typename CleanupFunc, typename WaitPolicy>
std::atomic<int> OnceInitializer<InitFunc, CleanupFunc, WaitPolicy>::state_{0};  // NOT_INITIALIZED

template<typename InitFunc, typename CleanupFunc, typename WaitPolicy>  
std::atomic<int> OnceInitializer<InitFunc, CleanupFunc, WaitPolicy>::reference_count_{0};

//------------------------------
// RAII 래퍼 클래스 - 생성/소멸자에서 자동 Initialize/Cleanup
//------------------------------
template<typename InitFunc = std::function<bool()>, typename CleanupFunc = std::function<void()>>
class OnceInitializerRAII
{
public:
    OnceInitializerRAII(InitFunc init_func, CleanupFunc cleanup_func) 
        : cleanup_func_(cleanup_func)
    {
        if (!OnceInitializer<InitFunc, CleanupFunc>::Initialize(init_func)) {
            throw std::runtime_error("OnceInitializer initialization failed");
        }
    }
    
    ~OnceInitializerRAII()
    {
        OnceInitializer<InitFunc, CleanupFunc>::Cleanup(cleanup_func_);
    }
    
    // 복사/이동 금지 (명확한 생명주기를 위해)
    OnceInitializerRAII(const OnceInitializerRAII&) = delete;
    OnceInitializerRAII& operator=(const OnceInitializerRAII&) = delete;
    OnceInitializerRAII(OnceInitializerRAII&&) = delete;
    OnceInitializerRAII& operator=(OnceInitializerRAII&&) = delete;

private:
    CleanupFunc cleanup_func_;
};

//------------------------------
// 편의 타입 별칭들
//------------------------------
using DefaultOnceInitializer = OnceInitializer<>;  // 기본 (스핀락)
using SpinOnceInitializer = OnceInitializer<std::function<bool()>, std::function<void()>, SpinWaitPolicy>;
using MutexOnceInitializer = OnceInitializer<std::function<bool()>, std::function<void()>, MutexWaitPolicy>;
using HybridOnceInitializer = OnceInitializer<std::function<bool()>, std::function<void()>, HybridWaitPolicy>;