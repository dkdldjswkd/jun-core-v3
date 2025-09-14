#include <iostream>
#include <thread>
#include <vector>
#include "../JunCommon/synchronization/OnceInitializer.h"

//------------------------------
// OnceInitializer 테스트 예시
//------------------------------

// WSA 초기화 시뮬레이션
using WSAOnceInit = OnceInitializer<std::function<bool()>, std::function<void()>>;

bool TestWSAInitialization()
{
    printf("=== OnceInitializer 테스트 시작 ===\n");
    
    // 멀티스레드 환경에서 동시 초기화 테스트
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> init_call_count{0};
    
    auto init_func = [&init_call_count]() -> bool {
        init_call_count++;
        printf("[INIT] WSA 초기화 실행됨 (호출 횟수: %d)\n", init_call_count.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 초기화 지연 시뮬레이션
        return true;
    };
    
    auto cleanup_func = []() {
        printf("[CLEANUP] WSA 정리 실행됨\n");
    };
    
    // 10개 스레드에서 동시에 초기화 시도
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            if (WSAOnceInit::Initialize(init_func)) {
                success_count++;
                printf("[Thread %d] 초기화 성공\n", i);
            } else {
                printf("[Thread %d] 초기화 실패\n", i);
            }
        });
    }
    
    // 모든 스레드 종료 대기
    for (auto& t : threads) {
        t.join();
    }
    
    printf("초기화 호출 횟수: %d (예상: 1)\n", init_call_count.load());
    printf("성공한 스레드 수: %d (예상: 10)\n", success_count.load());
    printf("현재 참조 개수: %d\n", WSAOnceInit::GetReferenceCount());
    printf("현재 상태: %d (0=미초기화, 1=초기화중, 2=완료, -1=실패)\n", WSAOnceInit::GetState());
    
    // 정리 테스트 - 각 스레드에서 Cleanup 호출
    threads.clear();
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            WSAOnceInit::Cleanup(cleanup_func);
            printf("[Thread %d] 정리 완료\n", i);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    printf("최종 참조 개수: %d (예상: 0)\n", WSAOnceInit::GetReferenceCount());
    printf("초기화 상태: %s (예상: false)\n", WSAOnceInit::IsInitialized() ? "true" : "false");
    
    bool test_result = (init_call_count == 1) && (success_count == 10) && 
                      (WSAOnceInit::GetReferenceCount() == 0) && 
                      (!WSAOnceInit::IsInitialized());
    
    printf("=== 테스트 결과: %s ===\n", test_result ? "성공" : "실패");
    return test_result;
}

// RAII 래퍼 테스트
bool TestRAIIWrapper()
{
    printf("\n=== RAII 래퍼 테스트 시작 ===\n");
    
    using TestRAII = OnceInitializerRAII<std::function<bool()>, std::function<void()>>;
    
    auto init_func = []() -> bool {
        printf("[RAII] 초기화 실행\n");
        return true;
    };
    
    auto cleanup_func = []() {
        printf("[RAII] 정리 실행\n");
    };
    
    try {
        // 스코프 내에서 RAII 객체 생성
        {
            TestRAII wrapper1(init_func, cleanup_func);
            TestRAII wrapper2(init_func, cleanup_func); // 중복 초기화 방지 테스트
            
            printf("RAII 객체들 생성 완료\n");
        } // 스코프 종료 시 자동 정리
        
        printf("RAII 테스트 완료\n");
        return true;
    } catch (const std::exception& e) {
        printf("RAII 테스트 실패: %s\n", e.what());
        return false;
    }
}

// 정책별 테스트
bool TestPolicies()
{
    printf("\n=== 정책별 테스트 시작 ===\n");
    
    // 스핀락 정책
    using SpinInit = SpinOnceInitializer;
    printf("[SpinWaitPolicy] 테스트 중...\n");
    bool spin_result = SpinInit::Initialize([]() {
        printf("  스핀락 초기화 실행\n");
        return true;
    });
    SpinInit::ForceReset();
    
    // 하이브리드 정책  
    using HybridInit = HybridOnceInitializer;
    printf("[HybridWaitPolicy] 테스트 중...\n");
    bool hybrid_result = HybridInit::Initialize([]() {
        printf("  하이브리드 초기화 실행\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 약간의 지연
        return true;
    });
    HybridInit::ForceReset();
    
    // 뮤텍스 정책
    using MutexInit = MutexOnceInitializer;
    printf("[MutexWaitPolicy] 테스트 중...\n");
    bool mutex_result = MutexInit::Initialize([]() {
        printf("  뮤텍스 초기화 실행\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 긴 초기화 시뮬레이션
        return true;
    });
    MutexInit::ForceReset();
    
    bool all_policies_ok = spin_result && hybrid_result && mutex_result;
    printf("정책 테스트 결과: %s\n", all_policies_ok ? "성공" : "실패");
    return all_policies_ok;
}

void RunOnceInitializerTests()
{
    printf("OnceInitializer 테스트를 시작합니다...\n\n");
    
    bool test1 = TestWSAInitialization();
    bool test2 = TestRAIIWrapper();
    bool test3 = TestPolicies();
    
    printf("\n=== 전체 테스트 결과 ===\n");
    printf("기본 OnceInitializer 테스트: %s\n", test1 ? "성공" : "실패");
    printf("RAII 래퍼 테스트: %s\n", test2 ? "성공" : "실패");
    printf("정책 패턴 테스트: %s\n", test3 ? "성공" : "실패");
    printf("전체 결과: %s\n", (test1 && test2 && test3) ? "성공" : "실패");
}