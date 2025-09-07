#include "ServerSocketManager.h"
#include <iostream>

//------------------------------
// ServerSocketManager 구현
//------------------------------

bool ServerSocketManager::StartServer(const char* bindIP, WORD port, DWORD maxSessions)
{
    try 
    {
        // 1. 세션 배열 초기화
        InitializeSessions(maxSessions);
        
        // 2. 서버 소켓 생성
        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET) 
        {
            printf("Socket creation failed: %d\n", WSAGetLastError());
            return false;
        }
        
        // 3. 소켓 옵션 설정
        int optval = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        
        // 4. 주소 설정
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        
        if (bindIP && strlen(bindIP) > 0) 
        {
            inet_pton(AF_INET, bindIP, &serverAddr.sin_addr);
        } else 
        {
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        }
        
        // 5. 바인드
        if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
        {
            printf("Bind failed: %d\n", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 6. 리스닝 시작
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) 
        {
            printf("Listen failed: %d\n", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 7. Accept 스레드 시작
        running = true;
        acceptThread = std::thread(&ServerSocketManager::AcceptThreadFunc, this);
        
        LOG_INFO("Server started on %s:%d (Max Sessions: %lu)", bindIP ? bindIP : "0.0.0.0", port, maxSessions);
        return true;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("StartServer exception: %s", e.what());
        return false;
    }
}

void ServerSocketManager::StopServer()
{
    if (!running.exchange(false)) {
        return; // 이미 정지됨
    }
    
    printf("ServerSocketManager stopping...\n");
    
    // Accept 스레드 종료
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    
    // 세션 정리
    CleanupSessions();
    
    printf(" ServerSocketManager stopped\n");
}

void ServerSocketManager::AcceptThreadFunc()
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

bool ServerSocketManager::OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr)
{
    // 세션 할당
    Session* session = AllocateSession();
    {
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
        SessionId sessionId(static_cast<WORD>(session - &sessions[0]), GetTickCount());
        session->Set(clientSocket, clientIP, clientPort, sessionId, netBaseHandler);
    }
    
	// IOCP에 SOCKET 등록
	if (!iocpManager->RegisterSocket(clientSocket, session))
	{
		LOG_ERROR("IOCP registration failed for session %lld", session->session_id_.sessionId);
		// Session은 IOCount가 0이 되면 자동으로 Pool에 반환됨
        session->DecrementIOCount();
		return false;
	}

	// RECV 등록
	if (!iocpManager->PostAsyncReceive(session))
	{
		session->DecrementIOCount();
		return false;
	}

	netBaseHandler->OnClientJoin(session);
	currentSessionCount++;
	session->DecrementIOCount();
	return true;
}

void ServerSocketManager::OnClientDisconnect(Session* session)
{
    currentSessionCount--;
    netBaseHandler->OnClientLeave(session);
    // Session은 IOCount가 0이 되면 자동으로 Pool에 반환되므로 수동 해제 불필요
}

void ServerSocketManager::InitializeSessions(DWORD maxSessions)
{
    sessions.resize(maxSessions);
    
    // 세션 인덱스 스택 초기화 (역순으로 푸시)
    for (int i = maxSessions - 1; i >= 0; i--) {
        sessionIndexStack.Push(i);
        
        // 각 세션에 Pool 정보 직접 설정 (간단하고 명확함)
        sessions[i].SetSessionPool(&sessions, &sessionIndexStack);
    }
    
    printf("Sessions initialized: %lu\n", maxSessions);
}

void ServerSocketManager::CleanupSessions()
{
    sessions.clear();
    currentSessionCount = 0;
}

Session* ServerSocketManager::AllocateSession()
{
    DWORD index;
    if (!sessionIndexStack.Pop(&index)) 
    {
        return nullptr;
    }
    
    return &sessions[index];
}

void ServerSocketManager::ReleaseSession(Session* session)
{
    if (!session || sessions.empty()) {
        return;
    }
    
    // pool의 session인지 검사
    if (session < &sessions[0] || session >= &sessions[0] + sessions.size()) 
    {
        return;
    }
    
    // Session 자체의 Release 함수 호출
    session->Release();
    
    // 인덱스 반환 (SessionManager의 책임)
    DWORD index = (DWORD)(session - &sessions[0]);
    sessionIndexStack.Push(index);
}

void ServerSocketManager::ReturnSessionToPool(Session* session)
{
    if (!session || sessions.empty()) {
        return;
    }
    
    // pool의 session인지 검사
    if (session < &sessions[0] || session >= &sessions[0] + sessions.size()) 
    {
        return;
    }
    
    // 인덱스 반환 (Session::Release()는 이미 호출됨)
    DWORD index = (DWORD)(session - &sessions[0]);
    sessionIndexStack.Push(index);
}