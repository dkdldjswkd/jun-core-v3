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
        // 소켓 생성
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
        
        // AcceptEx 함수 포인터 로딩
        if (!LoadAcceptExFunctions())
        {
            LOG_ERROR("Failed to load AcceptEx functions");
            closesocket(listenSocket);
            return false;
        }
        
        // 리슨 소켓을 IOCP에 등록
        if (!iocpManager->RegisterSocket(listenSocket))
        {
            LOG_ERROR("Failed to register listen socket to IOCP");
            closesocket(listenSocket);
            return false;
        }
        
        // 초기 AcceptEx 등록
        running = true;
        if (!PostAcceptEx())
        {
            LOG_ERROR("Failed to post initial AcceptEx");
            running = false;
            closesocket(listenSocket);
            return false;
        }
        
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
    
    // 리슨 소켓 정리
    if (listenSocket != INVALID_SOCKET) 
    {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    
    // 사용자 콜백 호출
    OnServerStop();
}


bool Server::LoadAcceptExFunctions()
{
    // AcceptEx 함수 포인터 로딩
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD dwBytes = 0;
    
    int result = WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                         &guidAcceptEx, sizeof(guidAcceptEx),
                         &fnAcceptEx, sizeof(fnAcceptEx),
                         &dwBytes, NULL, NULL);
    
    if (result == SOCKET_ERROR)
    {
        LOG_ERROR("Failed to load AcceptEx function pointer: %d", WSAGetLastError());
        return false;
    }
    
    // GetAcceptExSockaddrs 함수 포인터 로딩
    GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    
    result = WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
                     &fnGetAcceptExSockaddrs, sizeof(fnGetAcceptExSockaddrs),
                     &dwBytes, NULL, NULL);
    
    if (result == SOCKET_ERROR)
    {
        LOG_ERROR("Failed to load GetAcceptExSockaddrs function pointer: %d", WSAGetLastError());
        return false;
    }
    
    return true;
}

bool Server::PostAcceptEx()
{
    if (!fnAcceptEx)
    {
        LOG_ERROR("AcceptEx function pointer not loaded");
        return false;
    }
    
    // 새 클라이언트 소켓 생성 (WSA_FLAG_OVERLAPPED 플래그 추가)
    SOCKET acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (acceptSocket == INVALID_SOCKET)
    {
        LOG_ERROR("Failed to create accept socket: %d", WSAGetLastError());
        return false;
    }
    
    // Accept 소켓을 IOCP에 등록
    if (!iocpManager->RegisterSocket(acceptSocket))
    {
        LOG_ERROR("Failed to register accept socket to IOCP: %d", GetLastError());
        closesocket(acceptSocket);
        return false;
    }
    
    // Session 사전 생성 및 설정
    auto session = std::make_shared<Session>();
    session->sock_ = acceptSocket;
    session->SetEngine(this);
    
    // OverlappedEx 생성
    auto acceptOverlapped = new OverlappedEx(session, IOOperation::IO_ACCEPT);
    
    // AcceptEx 호출 (OverlappedEx 내장 버퍼 사용)
    DWORD bytesReceived = 0;
    BOOL result = fnAcceptEx(
        listenSocket,           // 리슨 소켓
        acceptSocket,           // Accept할 소켓
        acceptOverlapped->acceptAddressBuffer, // OverlappedEx 내장 주소 버퍼
        0,                      // 수신 데이터 길이 (0으로 설정)
        sizeof(SOCKADDR_IN) + 16, // 로컬 주소 길이
        sizeof(SOCKADDR_IN) + 16, // 원격 주소 길이
        &bytesReceived,         // 수신된 바이트 수
        &acceptOverlapped->overlapped_ // OVERLAPPED 구조체
    );
    
    DWORD lastError = WSAGetLastError();
    if (!result && lastError != ERROR_IO_PENDING)
    {
        LOG_ERROR("AcceptEx failed: result=%d, error=%d", result, lastError);
        closesocket(acceptSocket);
        delete acceptOverlapped;
        return false;
    }
    
    return true;
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
        session->RecvAsync();
    }
    else
    {
        LOG_ERROR("Session allocation failed - closing connection");
        closesocket(clientSocket);
        return false;
    }

	return true;
}
