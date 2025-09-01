#pragma once
#include "../core/WindowsIncludes.h"
#include <memory>
#include <vector>
#include <thread>
#include "NetworkEngine.h"
#include "IOCPResource.h"
#include "../../JunCommon/queue/JobQueue.h"
#include "../../JunCommon/queue/PacketJob.h"

//------------------------------
// NetworkManager - 단일 IOCP 기반 멀티 엔진 관리
// 단일 공유 IOCP로 모든 네트워크 엔진을 통합 관리하되, 
// 우선순위에 따라 HandlerThreadPool 자원을 차등 배분
//------------------------------
class NetworkManager
{
public:
    NetworkManager();
    ~NetworkManager();

    // 공유 IOCP 생성 및 관리
    void CreateSharedIOCP(DWORD totalWorkerThreads = 5);  // CPU 코어 수에 맞춤
    
    // 우선순위별 HandlerThreadPool 생성
    void CreateServerHandlerPool(DWORD handlerThreads = 4);    // 높은 우선순위
    void CreateClientHandlerPool(DWORD handlerThreads = 1);    // 낮은 우선순위
    
    // 엔진 추가 (서버용) - 고성능 HandlerPool 사용
    template<typename ServerEngineType>
    ServerEngineType* AddServerEngine(const char* systemFile, const char* configSection);
    
    // 엔진 추가 (클라이언트용) - 저성능 HandlerPool 사용
    template<typename ClientEngineType>
    ClientEngineType* AddClientEngine(const char* systemFile, const char* configSection);
    
    // 더미 클라이언트 추가 (대량 세션 테스트용)
    template<typename ClientEngineType>
    ClientEngineType* AddDummyClient(const char* systemFile, const char* configSection, 
                                   size_t maxSessions, DWORD connectInterval = 100);
    
    // 모든 엔진 시작/정지
    void StartAll();
    void StopAll();
    
    // TPS 모니터링
    void UpdateAllTPS();
    void PrintTPS() const;
    
    // Getter
    const IOCPResource* GetSharedIOCPResource() const { return sharedIOCPResource.get(); }
    HANDLE GetSharedIOCP() const { 
        return sharedIOCPResource ? sharedIOCPResource->GetHandle() : INVALID_HANDLE_VALUE; 
    }
    
    // HandlerPool Getter
    ThreadPool* GetServerHandlerPool() const { return serverHandlerPool.get(); }
    ThreadPool* GetClientHandlerPool() const { return clientHandlerPool.get(); }
    
    // Job 제출 인터페이스
    void SubmitServerJob(Job&& job);
    void SubmitClientJob(Job&& job);
    void SubmitServerPacketJob(PacketJob&& packetJob, DWORD priority = 0);
    void SubmitClientPacketJob(PacketJob&& packetJob, DWORD priority = 0);
    
private:
    // 새로운 IOCP Resource 기반 관리
    std::unique_ptr<IOCPResource> sharedIOCPResource;
    
    // 우선순위별 핸들러 스레드 풀
    std::unique_ptr<ThreadPool> serverHandlerPool;    // 높은 우선순위 (4 스레드)
    std::unique_ptr<ThreadPool> clientHandlerPool;    // 낮은 우선순위 (1 스레드)
    
    // 엔진 관리 (통합)
    std::vector<std::unique_ptr<NetBase>> allEngines;
    
private:
    void CleanupIOCP();
};

//------------------------------
// NetworkManager 템플릿 구현
//------------------------------

template<typename ServerEngineType>
inline ServerEngineType* NetworkManager::AddServerEngine(const char* systemFile, const char* configSection)
{
    static_assert(std::is_base_of_v<NetBase, ServerEngineType>, "ServerEngineType must inherit from NetBase");
    
    if (!sharedIOCPResource)
    {
        CreateSharedIOCP();  // 공유 IOCP 생성
    }
    
    auto engine = std::make_unique<ServerEngineType>(systemFile, configSection);
    
    // 새로운 Resource 기반 IOCP 연결
    engine->AttachToIOCP(*sharedIOCPResource);
    
    // NetworkManager 참조 설정 (PacketJob 제출용)
    engine->SetNetworkManager(this);
    
    ServerEngineType* rawPtr = engine.get();
    allEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}

template<typename ClientEngineType>
inline ClientEngineType* NetworkManager::AddClientEngine(const char* systemFile, const char* configSection)
{
    static_assert(std::is_base_of_v<NetBase, ClientEngineType>, "ClientEngineType must inherit from NetBase");
    
    if (!sharedIOCPResource)
    {
        CreateSharedIOCP();  // 공유 IOCP 생성
    }
    
    auto engine = std::make_unique<ClientEngineType>(systemFile, configSection);
    
    // 새로운 Resource 기반 IOCP 연결
    engine->AttachToIOCP(*sharedIOCPResource);
    
    // NetworkManager 참조 설정 (PacketJob 제출용)
    engine->SetNetworkManager(this);
    
    ClientEngineType* rawPtr = engine.get();
    allEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}

template<typename ClientEngineType>
inline ClientEngineType* NetworkManager::AddDummyClient(const char* systemFile, const char* configSection,
                                                       size_t maxSessions, DWORD connectInterval)
{
    static_assert(std::is_base_of_v<NetBase, ClientEngineType>, "ClientEngineType must inherit from NetBase");
    
    if (!sharedIOCPResource)
    {
        CreateSharedIOCP();  // 공유 IOCP 생성
    }
    
    auto engine = std::make_unique<ClientEngineType>(systemFile, configSection);
    
    // 더미 클라이언트 설정 (PolicyData에 직접 접근하여 설정)
    auto& policyData = engine->GetPolicyData();
    policyData.maxSessions = maxSessions;
    policyData.isDummyClient = true;
    policyData.connectInterval = connectInterval;
    
    // 세션 벡터 재할당
    policyData.sessions.resize(maxSessions);
    
    // 인덱스 스택 재구성
    while (policyData.availableIndexes.GetUseCount() > 0) {
        size_t dummy;
        policyData.availableIndexes.Pop(&dummy);
    }
    for (size_t i = maxSessions; i > 0; --i) {
        policyData.availableIndexes.Push(i - 1);
    }
    
    // 새로운 Resource 기반 IOCP 연결
    engine->AttachToIOCP(*sharedIOCPResource);
    
    // NetworkManager 참조 설정 (PacketJob 제출용)
    engine->SetNetworkManager(this);
    
    ClientEngineType* rawPtr = engine.get();
    allEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}