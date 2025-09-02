#pragma once
#include "../core/WindowsIncludes.h"
#include <memory>
#include <vector>
#include <thread>
#include "NetBase.h"
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
    
    // 엔진 추가 (간단한 인터페이스)
    void AddServerEngine(std::unique_ptr<NetBase> engine);
    void AddClientEngine(std::unique_ptr<NetBase> engine);
    
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

// 간단한 구현으로 대체 예정