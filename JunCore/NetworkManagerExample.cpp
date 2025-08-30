#include "JunCore/network/NetworkManager.h"
#include "JunCore/network/NetworkEngine.h"
#include <iostream>
#include <thread>
#include <chrono>

//------------------------------
// 사용 예제
//------------------------------

class MyGameServer : public GameServer 
{
public:
    MyGameServer(const char* systemFile, const char* configSection) 
        : GameServer(systemFile, configSection) {}
    
    // Pure virtual function implementations
    bool OnConnectionRequest(in_addr ip, WORD port) override {
        return true; // Allow all connections
    }
    
    void OnClientJoin(SessionId sessionId) override {
        printf("Server: Client connected: %lld\n", sessionId.sessionId);
    }
    
    void OnRecv(SessionId sessionId, PacketBuffer* packet) override {
        // Echo back the packet
        PacketBuffer* responsePacket = PacketBuffer::Alloc();
        // Copy received data to response (simplified)
        SendPacket(sessionId, responsePacket);
    }
    
    void OnClientLeave(SessionId sessionId) override {
        printf("Server: Client disconnected: %lld\n", sessionId.sessionId);
    }
    
    void OnServerStop() override {
        printf("Server: Stopped\n");
    }
};

class MyGameClient : public GameClient 
{
public:
    MyGameClient(const char* systemFile, const char* configSection) 
        : GameClient(systemFile, configSection) {}
    
    // Client-specific callbacks
    void OnConnect() override {
        printf("Client: Connected to server\n");
    }
    
    void OnRecv(PacketBuffer* packet) override {
        printf("Client: Received packet\n");
        PacketBuffer::Free(packet);
    }
    
    void OnDisconnect() override {
        printf("Client: Disconnected from server\n");
    }
};

class MyDummyClient : public GameClient 
{
public:
    MyDummyClient(const char* systemFile, const char* configSection) 
        : GameClient(systemFile, configSection) {}
    
    void OnConnect() override {
        printf("DummyClient: Connected %zu sessions\n", 
               GetPolicyData().currentSessions);
    }
    
    void OnRecv(PacketBuffer* packet) override {
        // 더미 클라이언트는 간단히 패킷 해제만
        PacketBuffer::Free(packet);
    }
    
    void OnDisconnect() override {
        printf("DummyClient: Disconnected\n");
    }
};

int main() 
{
    try 
    {
        NetworkManager manager;
        
        // 서버 IOCP (4개 스레드) 생성
        manager.CreateServerIOCP(4);
        
        // 클라이언트 IOCP (1개 스레드) 생성  
        manager.CreateClientIOCP(1);
        
        // 게임 서버 추가 (서버 IOCP 사용)
        auto* gameServer = manager.AddServerEngine<MyGameServer>(
            "ServerConfig.ini", "SERVER");
        
        // 일반 클라이언트 추가 (클라이언트 IOCP 사용)
        auto* loginClient = manager.AddClientEngine<MyGameClient>(
            "ClientConfig.ini", "LOGIN");
            
        auto* chatClient = manager.AddClientEngine<MyGameClient>(
            "ClientConfig.ini", "CHAT");
        
        // 더미 클라이언트 추가 (10,000개 세션으로 서버 테스트)
        auto* dummyClient = manager.AddDummyClient<MyDummyClient>(
            "DummyConfig.ini", "DUMMY", 10000, 10);  // 10ms 간격으로 연결
        
        // 모든 엔진 시작
        manager.StartAll();
        
        printf("=== NetworkManager Example Started ===\n");
        printf("Game Server: 4 shared worker threads (Server IOCP)\n");
        printf("Login + Chat + Dummy Clients: 1 shared worker thread (Client IOCP)\n");  
        printf("CompletionKey routing: Each session automatically calls correct handler\n");
        printf("User Experience: Just implement OnRecv, OnConnect, etc. - routing is automatic!\n");
        printf("========================================================================\n");
        
        // TPS 모니터링
        while (true) 
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            manager.UpdateAllTPS();
            manager.PrintTPS();
        }
    }
    catch (const std::exception& e) 
    {
        printf("Error: %s\n", e.what());
        return -1;
    }
    
    return 0;
}

/*
실행 결과 예상:
=== NetworkManager Example Started ===
Game Server: 4 threads (Server IOCP)  
Login + Chat Clients: 1 thread (Client IOCP)
Dummy Client: 10,000 sessions sharing 1 thread
=========================================

DummyClient: Connected 10000 sessions
Server: Client connected: 1
Server: Client connected: 2
...
Server: Client connected: 10000

=== NetworkManager TPS Report ===
Server Engines (1):
  [0] SendTPS:  15234, RecvTPS:  15156
Client Engines (3):  
  [0] SendTPS:     12, RecvTPS:     8    # Login Client
  [1] SendTPS:     34, RecvTPS:    28    # Chat Client  
  [2] SendTPS:  14892, RecvTPS: 14823    # Dummy Client (10K sessions)
=================================

핵심 장점:
1. 서버는 4개 스레드로 고성능 처리
2. 클라이언트들은 1개 스레드 공유 (스레드 간섭 없음)
3. 10,000개 더미 세션도 1개 스레드에서 효율적 처리
4. IOCP 분리로 서버 성능 보장
*/