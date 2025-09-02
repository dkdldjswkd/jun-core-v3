#pragma once
#include "IOCPManager.h"
#include "NetBase.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <typeinfo>

// NetworkArchitecture - 네트워크 아키텍처 통합 관리
// 단일 책임: IOCP와 핸들러들을 조화롭게 연결
class NetworkArchitecture final
{
private:
    // Core IOCP Manager (단일 인스턴스)
    std::shared_ptr<IOCPManager> coreIOCP;
    
    // 등록된 핸들러들
    std::vector<std::weak_ptr<NetBase>> registeredHandlers;
    
    // 핸들러별 설정 (향후 확장)
    std::unordered_map<std::string, std::string> handlerConfigs;

public:
    // Singleton Pattern - 전역 아키텍처
    static NetworkArchitecture& GetInstance() {
        static NetworkArchitecture instance;
        return instance;
    }
    
    // 복사/이동 금지
    NetworkArchitecture(const NetworkArchitecture&) = delete;
    NetworkArchitecture& operator=(const NetworkArchitecture&) = delete;
    NetworkArchitecture(NetworkArchitecture&&) = delete;
    NetworkArchitecture& operator=(NetworkArchitecture&&) = delete;

private:
    NetworkArchitecture() = default;
    ~NetworkArchitecture() = default;

public:
    // 아키텍처 초기화
    void Initialize(int workerThreadCount = 5) {
        if (!coreIOCP) {
            coreIOCP = IOCPManager::Create()
                .WithWorkerCount(workerThreadCount)
                .Build();
                
            printf("NetworkArchitecture initialized with %d worker threads\n", workerThreadCount);
        }
    }
    
    void Shutdown() {
        if (coreIOCP) {
            coreIOCP->Shutdown();
            coreIOCP.reset();
            registeredHandlers.clear();
            printf("NetworkArchitecture shutdown complete\n");
        }
    }

public:
    // 핸들러 등록/해제
    template<typename HandlerType>
    std::shared_ptr<HandlerType> CreateHandler() {
        if (!coreIOCP) {
            throw std::runtime_error("NetworkArchitecture not initialized");
        }
        
        auto handler = std::make_shared<HandlerType>();
        handler->AttachIOCPManager(coreIOCP);
        
        // 약한 참조로 등록 (순환 참조 방지)
        registeredHandlers.push_back(handler);
        
        // 설정 저장 (향후 재연결시 사용)
        std::string key = typeid(HandlerType).name();
        handlerConfigs[key] = typeid(HandlerType).name();
        
        printf("Handler registered: %s\n", typeid(HandlerType).name());
        return handler;
    }
    
    void UnregisterHandler(std::shared_ptr<NetBase> handler) {
        if (!handler) return;
        
        handler->DetachIOCPManager();
        
        // 만료된 약한 참조들 정리
        registeredHandlers.erase(
            std::remove_if(registeredHandlers.begin(), registeredHandlers.end(),
                [](const std::weak_ptr<NetBase>& weak) { return weak.expired(); }),
            registeredHandlers.end()
        );
        
        printf("Handler unregistered\n");
    }

public:
    // 모니터링 인터페이스
    size_t GetActiveHandlerCount() const {
        return std::count_if(registeredHandlers.begin(), registeredHandlers.end(),
            [](const std::weak_ptr<NetBase>& weak) { return !weak.expired(); });
    }
    
    bool IsInitialized() const noexcept {
        return coreIOCP != nullptr && coreIOCP->IsValid();
    }
    
    HANDLE GetIOCPHandle() const {
        return coreIOCP ? coreIOCP->GetHandle() : INVALID_HANDLE_VALUE;
    }

public:
    // 편의 함수들
    void UpdateAllTPS() {
        // 모니터링 관련 제거됨 - 빈 구현
    }
    
    // 고급 관리 기능 (향후 확장)
    void SetHandlerPriority(const std::string& handlerName, int priority) {
        // 향후 스레드 스케줄링에 활용
    }
    
    // 동적 워커 스레드 수 조정 (향후 구현)
    void AdjustWorkerThreadCount(int newCount) {
        // 런타임 워커 스레드 수 조정 기능
    }
};

// 편의 매크로들
#define NETWORK_ARCH NetworkArchitecture::GetInstance()

// 핸들러 생성 매크로
#define CREATE_HANDLER(HandlerType) \
    NETWORK_ARCH.CreateHandler<HandlerType>()

// 아키텍처 초기화 매크로
#define INIT_NETWORK_ARCH(WorkerCount) \
    NETWORK_ARCH.Initialize(WorkerCount)

// 아키텍처 종료 매크로  
#define SHUTDOWN_NETWORK_ARCH() \
    NETWORK_ARCH.Shutdown()

// 사용 예시 주석
/*

// 사용 방법

int main() {
    // 1. 아키텍처 초기화
    INIT_NETWORK_ARCH(8);
    
    // 2. 서버 핸들러 생성
    auto gameServer = CREATE_HANDLER(GameServer);
    
    // 3. 클라이언트 핸들러 생성  
    auto gameClient = CREATE_HANDLER(GameClient);
    
    // 4. 서버 시작
    gameServer->Start();
    
    // 5. 주기적 루프
    while (running) {
        Sleep(1000);
    }
    
    // 6. 정리
    gameServer->Stop();
    SHUTDOWN_NETWORK_ARCH();
    return 0;
}

*/