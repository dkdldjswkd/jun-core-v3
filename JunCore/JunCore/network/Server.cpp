#include "Server.h"
#include <iostream>

//------------------------------
// Server 구현 (기존 ServerSocketManager 로직 통합)
//------------------------------

bool Server::StartServer(const char* bindIP, WORD port, DWORD maxSessions)
{
    if (!IsInitialized())
    {
        LOG_ERROR("Must call Initialize() before StartServer()!");
        return false;
    }
    
    if (running.load())
    {
        LOG_WARN("Server is already running!");
        return false;
    }

    try 
    {
        // 1. 소켓 생성
        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET) 
        {
            LOG_ERROR("Socket creation failed: %d", WSAGetLastError());
            return false;
        }
        
        // 2. 소켓 옵션 세팅
        int optval = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        
        // 3. 주소 세팅
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
		inet_pton(AF_INET, bindIP, &serverAddr.sin_addr);
        
        // 4. 바인드
        if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
        {
            LOG_ERROR("Bind failed: %d", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 5. 리슨
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) 
        {
            LOG_ERROR("Listen failed: %d", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 6. Accept 스레드 시작
        running = true;
        acceptThread = std::thread(&Server::AcceptThreadFunc, this);
        
        OnServerStart();
        
        LOG_INFO("Server started on %s:%d (Max Sessions: %lu)", bindIP, port, maxSessions);
        return true;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("StartServer exception: %s", e.what());
        return false;
    }
}

void Server::StopServer()
{
    if (!running.exchange(false)) 
    {
        return; // 이미 정지됨
    }
    
    LOG_INFO("Server stopping...");
    
    // Accept 스레드 종료
    if (listenSocket != INVALID_SOCKET) 
    {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    
    if (acceptThread.joinable()) 
    {
        acceptThread.join();
    }
    
    // 사용자 콜백 호출
    OnServerStop();
    
    LOG_INFO("Server stopped successfully");
}

void Server::AcceptThreadFunc()
{
    LOG_INFO("Accept thread started");
    
    while (running.load()) 
    {
        SOCKADDR_IN clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        
        // Accept 대기
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) 
        {
            if (running.load()) 
            {
                LOG_ERROR("Accept failed: %d", WSAGetLastError());
            }
            continue;
        }
        
        // 클라이언트 연결 처리
        OnClientConnect(clientSocket, &clientAddr);
    }
    
	LOG_INFO("Accept thread ended");
}

bool Server::OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr)
{
    if (!iocpManager->RegisterSocket(clientSocket))
    {
        LOG_WARN("IOCP registration failed for socket 0x%llX", clientSocket);
        return false;
    }

    if (std::shared_ptr<Session> session = std::make_shared<Session>())
    {
        User* user = new User(session);
        session->Set(clientSocket, clientAddr->sin_addr, ntohs(clientAddr->sin_port), this/*NetEngine*/, user);
        OnSessionConnect(user);
        session->PostAsyncReceive();
    }
    else
    {
        LOG_ERROR("Session allocation failed - closing connection");
        closesocket(clientSocket);
        return false;
    }

	return true;
}
