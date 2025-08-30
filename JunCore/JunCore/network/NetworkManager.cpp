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

void NetworkManager::CreateServerIOCP(DWORD serverThreads)
{
    if (serverIOCP != INVALID_HANDLE_VALUE)
        return;  // 이미 생성됨
        
    serverThreadCount = serverThreads;
    serverIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, serverThreads);
    if (serverIOCP == INVALID_HANDLE_VALUE)
    {
        throw std::exception("Failed to create server IOCP");
    }
    
    // 공유 IOCP 워커 스레드 생성
    serverWorkerThreads.reserve(serverThreads);
    for (DWORD i = 0; i < serverThreads; ++i)
    {
        serverWorkerThreads.emplace_back([this]() {
            NetBase::RunSharedWorkerThread(serverIOCP);
        });
    }
    
    ownsServerIOCP = true;
}

void NetworkManager::CreateClientIOCP(DWORD clientThreads)
{
    if (clientIOCP != INVALID_HANDLE_VALUE)
        return;  // 이미 생성됨
        
    clientThreadCount = clientThreads;
    clientIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, clientThreads);
    if (clientIOCP == INVALID_HANDLE_VALUE)
    {
        throw std::exception("Failed to create client IOCP");
    }
    
    // 공유 IOCP 워커 스레드 생성
    clientWorkerThreads.reserve(clientThreads);
    for (DWORD i = 0; i < clientThreads; ++i)
    {
        clientWorkerThreads.emplace_back([this]() {
            NetBase::RunSharedWorkerThread(clientIOCP);
        });
    }
    
    ownsClientIOCP = true;
}

void NetworkManager::StartAll()
{
    // 모든 서버 엔진 시작
    for (auto& engine : serverEngines)
    {
        engine->Start();
    }
    
    // 모든 클라이언트 엔진 시작
    for (auto& engine : clientEngines)
    {
        engine->Start();
    }
}

void NetworkManager::StopAll()
{
    // 모든 서버 엔진 정지
    for (auto& engine : serverEngines)
    {
        engine->Stop();
    }
    
    // 모든 클라이언트 엔진 정지
    for (auto& engine : clientEngines)
    {
        engine->Stop();
    }
    
    // 워커 스레드 종료 신호
    if (ownsServerIOCP && serverIOCP != INVALID_HANDLE_VALUE)
    {
        for (size_t i = 0; i < serverWorkerThreads.size(); ++i)
        {
            PostQueuedCompletionStatus(serverIOCP, 0, 0, 0);
        }
    }
    
    if (ownsClientIOCP && clientIOCP != INVALID_HANDLE_VALUE)
    {
        for (size_t i = 0; i < clientWorkerThreads.size(); ++i)
        {
            PostQueuedCompletionStatus(clientIOCP, 0, 0, 0);
        }
    }
    
    // 워커 스레드 종료 대기
    for (auto& thread : serverWorkerThreads)
    {
        if (thread.joinable())
            thread.join();
    }
    
    for (auto& thread : clientWorkerThreads)
    {
        if (thread.joinable())
            thread.join();
    }
    
    serverWorkerThreads.clear();
    clientWorkerThreads.clear();
}

void NetworkManager::UpdateAllTPS()
{
    for (auto& engine : serverEngines)
    {
        engine->UpdateTPS();
    }
    
    for (auto& engine : clientEngines)
    {
        engine->UpdateTPS();
    }
}

void NetworkManager::PrintTPS() const
{
    std::cout << "=== NetworkManager TPS Report ===" << std::endl;
    
    std::cout << "Server Engines (" << serverEngines.size() << "):" << std::endl;
    for (size_t i = 0; i < serverEngines.size(); ++i)
    {
        const auto& engine = serverEngines[i];
        std::cout << "  [" << i << "] SendTPS: " << std::setw(6) << engine->GetSendTPS()
                  << ", RecvTPS: " << std::setw(6) << engine->GetRecvTPS() << std::endl;
    }
    
    std::cout << "Client Engines (" << clientEngines.size() << "):" << std::endl;
    for (size_t i = 0; i < clientEngines.size(); ++i)
    {
        const auto& engine = clientEngines[i];
        std::cout << "  [" << i << "] SendTPS: " << std::setw(6) << engine->GetSendTPS()
                  << ", RecvTPS: " << std::setw(6) << engine->GetRecvTPS() << std::endl;
    }
    
    std::cout << "=================================" << std::endl;
}

void NetworkManager::CleanupIOCP()
{
    if (ownsServerIOCP && serverIOCP != INVALID_HANDLE_VALUE)
    {
        CloseHandle(serverIOCP);
        serverIOCP = INVALID_HANDLE_VALUE;
        ownsServerIOCP = false;
    }
    
    if (ownsClientIOCP && clientIOCP != INVALID_HANDLE_VALUE)
    {
        CloseHandle(clientIOCP);
        clientIOCP = INVALID_HANDLE_VALUE;
        ownsClientIOCP = false;
    }
}