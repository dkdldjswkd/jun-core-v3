#pragma once
#include "IOCPManager.h"
#include "NetBase_New.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <typeinfo>

//------------------------------
// NetworkArchitecture - 아름다운 네트워크 아키텍처 통합 관리
// 단일 책임: "IOCP와 핸들러들을 조화롭게 연결한다"
//------------------------------
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
    //------------------------------
    // Singleton Pattern - 아름다운 전역 아키텍처
    //------------------------------
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
    //------------------------------
    // 아키텍처 초기화
    //------------------------------
    void Initialize(int workerThreadCount = 5) {
        if (!coreIOCP) {
            coreIOCP = IOCPManager::Create()
                .WithWorkerCount(workerThreadCount)
                .Build();
                
            printf("🚀 NetworkArchitecture initialized with %d worker threads\n", workerThreadCount);
        }
    }
    
    void Shutdown() {
        if (coreIOCP) {
            coreIOCP->Shutdown();
            coreIOCP.reset();
            registeredHandlers.clear();
            printf("🛑 NetworkArchitecture shutdown complete\n");
        }
    }

public:
    //------------------------------
    // 핸들러 등록/해제 - 아름다운 생명주기 관리
    //------------------------------
    template<typename HandlerType>
    std::shared_ptr<HandlerType> CreateHandler(const char* systemFile, const char* configSection) {
        if (!coreIOCP) {
            throw std::runtime_error("NetworkArchitecture not initialized");
        }
        
        auto handler = std::make_shared<HandlerType>(systemFile, configSection);
        handler->AttachIOCPManager(coreIOCP);
        
        // 약한 참조로 등록 (순환 참조 방지)
        registeredHandlers.push_back(handler);
        
        // 설정 저장 (향후 재연결시 사용)
        std::string key = std::string(systemFile) + "::" + configSection;
        handlerConfigs[key] = typeid(HandlerType).name();
        
        printf("📦 Handler registered: %s::%s\n", systemFile, configSection);
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
        
        printf("📤 Handler unregistered\n");
    }

public:
    //------------------------------
    // 모니터링 인터페이스
    //------------------------------
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
    //------------------------------
    // 편의 함수들 - 아름다운 사용자 경험
    //------------------------------
    
    // 모든 핸들러 TPS 업데이트
    void UpdateAllTPS() {
        for (auto& weak : registeredHandlers) {
            if (auto handler = weak.lock()) {
                handler->UpdateTPS();
            }
        }
    }
    
    // 전체 패킷 처리량 합계
    DWORD GetTotalPacketTPS() const {
        DWORD total = 0;
        for (auto& weak : registeredHandlers) {
            if (auto handler = weak.lock()) {
                total += handler->GetPacketTPS();
            }
        }
        return total;
    }
    
    // 전체 세션 수 합계
    DWORD GetTotalSessionCount() const {
        DWORD total = 0;
        for (auto& weak : registeredHandlers) {
            if (auto handler = weak.lock()) {
                total += handler->GetSessionCount();
            }
        }
        return total;
    }

public:
    //------------------------------
    // 고급 관리 기능 (향후 확장)
    //------------------------------
    
    // 핸들러별 우선순위 설정 (향후 구현)
    void SetHandlerPriority(const std::string& handlerName, int priority) {
        // 향후 스레드 스케줄링에 활용
    }
    
    // 동적 워커 스레드 수 조정 (향후 구현)
    void AdjustWorkerThreadCount(int newCount) {
        // 런타임 워커 스레드 수 조정 기능
    }
};

//------------------------------
// 편의 매크로들 - 아름다운 사용성
//------------------------------

// 전역 아키텍처 접근
#define NETWORK_ARCH NetworkArchitecture::GetInstance()

// 핸들러 생성 매크로
#define CREATE_HANDLER(HandlerType, SystemFile, ConfigSection) \
    NETWORK_ARCH.CreateHandler<HandlerType>(SystemFile, ConfigSection)

// 아키텍처 초기화 매크로
#define INIT_NETWORK_ARCH(WorkerCount) \
    NETWORK_ARCH.Initialize(WorkerCount)

// 아키텍처 종료 매크로  
#define SHUTDOWN_NETWORK_ARCH() \
    NETWORK_ARCH.Shutdown()

//------------------------------
// 사용 예시 주석
//------------------------------
/*

// 🚀 아름다운 사용 방법

int main() {
    // 1. 아키텍처 초기화
    INIT_NETWORK_ARCH(8);
    
    // 2. 서버 핸들러 생성
    auto gameServer = CREATE_HANDLER(GameServer, "SystemConfig.ini", "GAME_SERVER");
    
    // 3. 클라이언트 핸들러 생성  
    auto gameClient = CREATE_HANDLER(GameClient, "SystemConfig.ini", "GAME_CLIENT");
    
    // 4. 서버 시작
    gameServer->Start();
    
    // 5. 주기적 모니터링
    while (running) {
        NETWORK_ARCH.UpdateAllTPS();
        printf("Total TPS: %lu, Sessions: %lu\n", 
               NETWORK_ARCH.GetTotalPacketTPS(),
               NETWORK_ARCH.GetTotalSessionCount());
        Sleep(1000);
    }
    
    // 6. 정리
    gameServer->Stop();
    SHUTDOWN_NETWORK_ARCH();
    return 0;
}

*/