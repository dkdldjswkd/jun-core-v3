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
        // 1. 세션 세팅
        InitializeSessions(maxSessions);
        
        // 2. 소켓 생성
        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET) 
        {
            LOG_ERROR("Socket creation failed: %d", WSAGetLastError());
            return false;
        }
        
        // 3. 소켓 옵션 세팅
        int optval = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        
        // 4. 주소 세팅
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
		inet_pton(AF_INET, bindIP, &serverAddr.sin_addr);
        
        // 5. 바인드
        if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
        {
            LOG_ERROR("Bind failed: %d", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 6. 리슨
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) 
        {
            LOG_ERROR("Listen failed: %d", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 7. Accept 스레드 시작
        running = true;
        acceptThread = std::thread(&Server::AcceptThreadFunc, this);
        
        // 8. 사용자 콜백 호출
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
    if (!running.exchange(false)) {
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
    
    // 세션 정리
    CleanupSessions();
    
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
    // 세션 할당
    Session* session = AllocateSession();
    if (!session) 
    {
        LOG_ERROR("Session allocation failed - closing connection");
        closesocket(clientSocket);
        return false;
    }

    // 다른 스레드에서 정리되지 않도록 IOCount를 미리 선점한다.
    session->IncrementIOCount();

    // 세션 초기화
    in_addr clientIP = clientAddr->sin_addr;
    WORD clientPort = ntohs(clientAddr->sin_port);
    session->Set(clientSocket, clientIP, clientPort, this);  // SessionId 제거
    
	// IOCP에 SOCKET 등록
	if (!iocpManager->RegisterSocket(clientSocket, session))
	{
		LOG_ERROR("IOCP registration failed for session 0x%llX", (uintptr_t)session);
		// Session은 IOCount가 0이 되면 자동으로 Pool에 반환됨
        session->DecrementIOCount();
		return false;
	}

	// RECV 등록
	if (!session->PostAsyncReceive())
	{
		session->DecrementIOCount();
		return false;
	}

	OnClientJoin(session);
	currentSessionCount++;
	session->DecrementIOCount();
	return true;
}

void Server::OnClientDisconnect(Session* session)
{
    currentSessionCount--;
    OnClientLeave(session);
    // Session은 IOCount가 0이 되면 자동으로 Pool에 반환되므로 수동 해제 불필요
}

void Server::InitializeSessions(DWORD maxSessions)
{
    sessions.resize(maxSessions);
    
    // 세션 인덱스 스택 초기화 (역순으로 푸시)
    for (int i = maxSessions - 1; i >= 0; i--) {
        sessionIndexStack.Push(i);
        
        // 세션 동적 생성
        sessions[i] = std::make_unique<Session>();
        
        // 각 세션에 Pool 정보 직접 설정 (간단하고 명확함)
        sessions[i]->SetSessionPool(&sessions, &sessionIndexStack);
    }
}

void Server::CleanupSessions()
{
    sessions.clear();
    currentSessionCount = 0;
}

Session* Server::AllocateSession()
{
    DWORD index;
    if (!sessionIndexStack.Pop(&index)) 
    {
        return nullptr;
    }
    
    return sessions[index].get();
}