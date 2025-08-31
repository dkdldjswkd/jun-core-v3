#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include <thread>
#include "NetworkEngine.h"
#include "../../JunCommon/queue/JobQueue.h"
#include "../../JunCommon/queue/PacketJob.h"

//------------------------------
// NetworkManager - IOCP 공유 및 멀티 엔진 관리
// 서버용 IOCP와 클라이언트용 IOCP를 분리하여 성능 최적화
//------------------------------
class NetworkManager
{
public:
    NetworkManager();
    ~NetworkManager();

    // IOCP 생성 및 관리
    void CreateServerIOCP(DWORD serverThreads = 4);
    void CreateClientIOCP(DWORD clientThreads = 1);
    
    // HandlerThreadPool 생성 및 관리
    void CreateServerHandlerPool(DWORD handlerThreads = 4);
    void CreateClientHandlerPool(DWORD handlerThreads = 1);
    
    // 엔진 추가 (서버용)
    template<typename ServerEngineType>
    ServerEngineType* AddServerEngine(const char* systemFile, const char* configSection);
    
    // 엔진 추가 (클라이언트용)
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
    HANDLE GetServerIOCP() const { return serverIOCP; }
    HANDLE GetClientIOCP() const { return clientIOCP; }
    
    // HandlerPool Getter
    ThreadPool* GetServerHandlerPool() const { return serverHandlerPool.get(); }
    ThreadPool* GetClientHandlerPool() const { return clientHandlerPool.get(); }
    
    // Job 제출 인터페이스
    void SubmitServerJob(Job&& job);
    void SubmitClientJob(Job&& job);
    void SubmitServerPacketJob(PacketJob&& packetJob, DWORD priority = 0);
    void SubmitClientPacketJob(PacketJob&& packetJob, DWORD priority = 0);
    
private:
    // IOCP 핸들
    HANDLE serverIOCP = INVALID_HANDLE_VALUE;
    HANDLE clientIOCP = INVALID_HANDLE_VALUE;
    
    // 스레드 수
    DWORD serverThreadCount = 4;
    DWORD clientThreadCount = 1;
    
    // IOCP 워커 스레드
    std::vector<std::thread> serverWorkerThreads;
    std::vector<std::thread> clientWorkerThreads;
    
    // 핸들러 스레드 풀
    std::unique_ptr<ThreadPool> serverHandlerPool;
    std::unique_ptr<ThreadPool> clientHandlerPool;
    
    // 엔진 관리
    std::vector<std::unique_ptr<NetBase>> serverEngines;
    std::vector<std::unique_ptr<NetBase>> clientEngines;
    
    // 소유권 관리
    bool ownsServerIOCP = false;
    bool ownsClientIOCP = false;
    
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
    
    if (serverIOCP == INVALID_HANDLE_VALUE)
    {
        CreateServerIOCP();  // 기본 서버 IOCP 생성
    }
    
    auto engine = std::make_unique<ServerEngineType>(systemFile, configSection);
    
    // IOCP 설정 (Policy Initialize 후에 설정)
    engine->InitializeIOCP(serverIOCP);
    
    ServerEngineType* rawPtr = engine.get();
    serverEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}

template<typename ClientEngineType>
inline ClientEngineType* NetworkManager::AddClientEngine(const char* systemFile, const char* configSection)
{
    static_assert(std::is_base_of_v<NetBase, ClientEngineType>, "ClientEngineType must inherit from NetBase");
    
    if (clientIOCP == INVALID_HANDLE_VALUE)
    {
        CreateClientIOCP();  // 기본 클라이언트 IOCP 생성
    }
    
    auto engine = std::make_unique<ClientEngineType>(systemFile, configSection);
    
    // IOCP 설정
    engine->InitializeIOCP(clientIOCP);
    
    ClientEngineType* rawPtr = engine.get();
    clientEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}

template<typename ClientEngineType>
inline ClientEngineType* NetworkManager::AddDummyClient(const char* systemFile, const char* configSection,
                                                       size_t maxSessions, DWORD connectInterval)
{
    static_assert(std::is_base_of_v<NetBase, ClientEngineType>, "ClientEngineType must inherit from NetBase");
    
    if (clientIOCP == INVALID_HANDLE_VALUE)
    {
        CreateClientIOCP();  // 기본 클라이언트 IOCP 생성
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
    
    // IOCP 설정
    engine->InitializeIOCP(clientIOCP);
    
    ClientEngineType* rawPtr = engine.get();
    clientEngines.emplace_back(std::move(engine));
    
    return rawPtr;
}