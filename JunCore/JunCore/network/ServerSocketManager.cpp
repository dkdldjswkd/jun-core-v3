#include "ServerSocketManager.h"
#include <iostream>

//------------------------------
// ServerSocketManager 구현
//------------------------------

bool ServerSocketManager::StartServer(const char* bindIP, WORD port, DWORD maxSessions)
{
    printf("🚀 ServerSocketManager starting...\n");
    
    try {
        // 1. 세션 배열 초기화
        InitializeSessions(maxSessions);
        
        // 2. 서버 소켓 생성
        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET) {
            printf("❌ Socket creation failed: %d\n", WSAGetLastError());
            return false;
        }
        
        // 3. 소켓 옵션 설정
        int optval = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        
        // 4. 주소 설정
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        
        if (bindIP && strlen(bindIP) > 0) {
            inet_pton(AF_INET, bindIP, &serverAddr.sin_addr);
        } else {
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        }
        
        // 5. 바인드
        if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            printf("❌ Bind failed: %d\n", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 6. 리스닝 시작
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            printf("❌ Listen failed: %d\n", WSAGetLastError());
            closesocket(listenSocket);
            return false;
        }
        
        // 7. Accept 스레드 시작
        running = true;
        acceptThread = std::thread(&ServerSocketManager::AcceptThreadFunc, this);
        
        printf("✅ Server started on %s:%d (Max Sessions: %lu)\n", 
               bindIP ? bindIP : "0.0.0.0", port, maxSessions);
        return true;
    }
    catch (const std::exception& e) {
        printf("❌ StartServer exception: %s\n", e.what());
        return false;
    }
}

void ServerSocketManager::StopServer()
{
    if (!running.exchange(false)) {
        return; // 이미 정지됨
    }
    
    printf("🛑 ServerSocketManager stopping...\n");
    
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
    
    printf("✅ ServerSocketManager stopped\n");
}

void ServerSocketManager::AcceptThreadFunc()
{
    printf("🔄 Accept thread started\n");
    
    while (running.load()) {
        SOCKADDR_IN clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        
        // Accept 대기
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) {
            if (running.load()) {
                printf("⚠️ Accept failed: %d\n", WSAGetLastError());
            }
            continue;
        }
        
        // 클라이언트 연결 처리
        OnClientConnect(clientSocket, &clientAddr);
    }
    
    printf("🔄 Accept thread ended\n");
}

void ServerSocketManager::OnClientConnect(SOCKET clientSocket, SOCKADDR_IN* clientAddr)
{
    // 세션 할당
    Session* session = AllocateSession();
    if (!session) {
        printf("❌ Session allocation failed - closing connection\n");
        closesocket(clientSocket);
        return;
    }
    
    // 세션 초기화
    session->sock = clientSocket;
    session->sessionId.SESSION_INDEX = (WORD)(session - sessionArray);  // 세션 인덱스
    session->sessionId.SESSION_UNIQUE = GetTickCount();  // 유니크 ID (간단한 방법)
    session->disconnectFlag = FALSE;
    session->ioCount = 0;
    session->sendFlag = FALSE;
    session->sendPacketCount = 0;
    session->lastRecvTime = GetTickCount64();
    
    // 송수신 버퍼 초기화
    session->recvBuf.Clear();
    
    // sendQ 청소 (Clear 메서드 없음)
    PacketBuffer* packet;
    while (session->sendQ.Dequeue(&packet)) {
        PacketBuffer::Free(packet);
    }
    
    // Session에 엔진 포인터 설정 (IOCPManager가 세션에서 핸들러를 찾을 수 있도록)
    session->SetEngine(netBaseHandler);
    
    // IOCP에 소켓 등록 🔥 핵심 부분!
    if (!iocpManager->RegisterSocket(clientSocket, session)) {
        printf("❌ IOCP registration failed for session %lld\n", session->sessionId.sessionId);
        ReleaseSession(session);
        closesocket(clientSocket);
        return;
    }
    
    // 연결 성공
    currentSessionCount++;
    
    // 패킷 핸들러에 연결 알림
    netBaseHandler->OnSessionConnected(session);
    
    // 첫 번째 비동기 수신 등록 🚀
    iocpManager->StartAsyncReceive(session);
}

void ServerSocketManager::OnClientDisconnect(Session* session)
{
    currentSessionCount--;
    netBaseHandler->OnSessionDisconnected(session);
    ReleaseSession(session);
}

void ServerSocketManager::InitializeSessions(DWORD maxSessions)
{
    maxSessionCount = maxSessions;
    sessionArray = new Session[maxSessionCount];
    
    // 세션 인덱스 스택 초기화 (역순으로 푸시)
    for (int i = maxSessionCount - 1; i >= 0; i--) {
        sessionIndexStack.Push(i);
    }
    
    printf("📦 Sessions initialized: %lu\n", maxSessionCount);
}

void ServerSocketManager::CleanupSessions()
{
    if (sessionArray) {
        delete[] sessionArray;
        sessionArray = nullptr;
        maxSessionCount = 0;
        currentSessionCount = 0;
    }
}

Session* ServerSocketManager::AllocateSession()
{
    DWORD index;
    if (!sessionIndexStack.Pop(&index)) {
        return nullptr; // 사용 가능한 세션 없음
    }
    
    return &sessionArray[index];
}

void ServerSocketManager::ReleaseSession(Session* session)
{
    if (!session || session < sessionArray || session >= sessionArray + maxSessionCount) {
        return; // 잘못된 세션 포인터
    }
    
    // 소켓 정리
    if (session->sock != INVALID_SOCKET) {
        closesocket(session->sock);
        session->sock = INVALID_SOCKET;
    }
    
    // 송신 큐 정리
    PacketBuffer* packet;
    while (session->sendQ.Dequeue(&packet)) {
        PacketBuffer::Free(packet);
    }
    
    // 세션 초기화
    session->disconnectFlag = TRUE;
    session->ioCount = 0;
    session->sendFlag = FALSE;
    session->sendPacketCount = 0;
    
    // 인덱스 반환
    DWORD index = (DWORD)(session - sessionArray);
    sessionIndexStack.Push(index);
}