#include "NetworkManager.h"
#include <iostream>
#include <iomanip>

//------------------------------
// NetworkManager 구현
//------------------------------

NetworkManager::NetworkManager()
{
    // 생성자에서는 IOCP를 만들지 않고, 필요할 때 생성
}

NetworkManager::~NetworkManager()
{
    StopAll();
    CleanupIOCP();
}

void NetworkManager::CreateSharedIOCP(DWORD totalWorkerThreads)
{
    if (sharedIOCPResource)
        return;  // 이미 생성됨
    
    // IOCPResource Builder 패턴으로 생성
    sharedIOCPResource = IOCPResource::Builder()
        .WithWorkerCount(totalWorkerThreads)
        .WithWorkerFunction([](HANDLE iocp) {
            // 간단한 IOCP 워커 루프
            while (true) {
                DWORD bytesTransferred;
                ULONG_PTR completionKey;
                LPOVERLAPPED overlapped;
                
                if (!GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE)) {
                    break;
                }
                
                if (bytesTransferred == 0 && completionKey == 0 && overlapped == nullptr) {
                    break; // 종료 신호
                }
            }
        })
        .Build();
    
    // 기본 핸들러 풀들도 함께 생성
    CreateServerHandlerPool(4);  // 높은 우선순위
    CreateClientHandlerPool(1);  // 낮은 우선순위
}

void NetworkManager::StartAll()
{
    // 모든 엔진 시작 (통합)
    for (auto& engine : allEngines)
    {
        engine->Start();
    }
}

void NetworkManager::StopAll()
{
    // 모든 엔진 정지 (통합)
    for (auto& engine : allEngines)
    {
        engine->Stop();
    }
    
    // IOCPResource는 RAII로 자동 정리됨
    // 별도의 워커 스레드 관리가 불필요
}

void NetworkManager::CreateServerHandlerPool(DWORD handlerThreads)
{
    if (serverHandlerPool) return;  // 이미 생성됨
    
    serverHandlerPool = std::make_unique<ThreadPool>(handlerThreads, "ServerHandlerPool");
}

void NetworkManager::CreateClientHandlerPool(DWORD handlerThreads)
{
    if (clientHandlerPool) return;  // 이미 생성됨
    
    clientHandlerPool = std::make_unique<ThreadPool>(handlerThreads, "ClientHandlerPool");
}

void NetworkManager::SubmitServerJob(Job&& job)
{
    if (serverHandlerPool) {
        serverHandlerPool->Submit(std::move(job));
    }
}

void NetworkManager::SubmitClientJob(Job&& job)
{
    if (clientHandlerPool) {
        clientHandlerPool->Submit(std::move(job));
    }
}

void NetworkManager::SubmitServerPacketJob(PacketJob&& packetJob, DWORD priority)
{
    if (serverHandlerPool) {
        // PacketJobHandler는 각 엔진에서 구현하도록 함
        // 현재는 기본 Job으로 변환
        auto job = packetJob.ToJob([](const PacketJob&){
            // TODO: 실제 핸들러 연결 필요
        }, priority);
        serverHandlerPool->Submit(std::move(job));
    }
}

void NetworkManager::SubmitClientPacketJob(PacketJob&& packetJob, DWORD priority)
{
    if (clientHandlerPool) {
        // PacketJobHandler는 각 엔진에서 구현하도록 함
        // 현재는 기본 Job으로 변환
        auto job = packetJob.ToJob([](const PacketJob&){
            // TODO: 실제 핸들러 연결 필요
        }, priority);
        clientHandlerPool->Submit(std::move(job));
    }
}

void NetworkManager::CleanupIOCP()
{
    // 1. 모든 엔진의 IOCP 연결 해제
    for (auto& engine : allEngines) {
        engine->DetachIOCPManager();
    }
    
    // 2. HandlerPool 먼저 종료
    if (serverHandlerPool) {
        serverHandlerPool->Shutdown();
        serverHandlerPool.reset();
    }
    
    if (clientHandlerPool) {
        clientHandlerPool->Shutdown();
        clientHandlerPool.reset();
    }
    
    // 3. IOCPResource 자동 정리 (RAII)
    sharedIOCPResource.reset();
}

