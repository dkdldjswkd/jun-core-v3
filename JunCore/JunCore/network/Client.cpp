#include "Client.h"
#include "../log.h"

Client::Client(std::shared_ptr<IOCPManager> manager,
               const char* serverIP,
               WORD port,
               int targetConnectionCount)
    : NetBase(manager)
    , serverIP(serverIP)
    , serverPort(port)
    , targetConnectionCount_(targetConnectionCount)
{
}

Client::~Client()
{
    StopClient();
}

void Client::StartClient()
{
    if (!IsInitialized())
    {
        LOG_ERROR("Must call Initialize() before StartClient()!");
        return;
    }

    running_.store(true, std::memory_order_release);

    // ConnectEx 함수 포인터 로드
    if (!LoadConnectExFunctions())
    {
        LOG_ERROR("Failed to load ConnectEx functions");
        running_.store(false, std::memory_order_release);
        return;
    }

    // 재연결 스레드 시작
    reconnectThread_ = std::thread(&Client::ReconnectThreadFunc, this);

    // 초기 연결 시도
    LOG_INFO("Starting %d initial connections to %s:%d", targetConnectionCount_, serverIP.c_str(), serverPort);
    for (int i = 0; i < targetConnectionCount_; i++)
    {
        PostConnectEx();
    }
}

void Client::StopClient()
{
    if (!running_.load(std::memory_order_acquire))
    {
        return;
    }

    LOG_INFO("Stopping client");

    running_.store(false, std::memory_order_release);
    reconnectSignal_.release();  // 스레드 깨우기

    if (reconnectThread_.joinable())
    {
        reconnectThread_.join();
    }

    LOG_INFO("Client stopped");
}

void Client::ReconnectThreadFunc()
{
    LOG_DEBUG("Reconnect thread started");

    constexpr int RECONNECT_DELAY_MS = 1000;  // 재연결 시도 간격 1초

    while (running_.load(std::memory_order_acquire))
    {
        // 타임아웃 있는 대기: 서버 다운 시에도 1초마다만 재시도
        bool signaled = reconnectSignal_.try_acquire_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));

        if (!running_.load(std::memory_order_acquire))
        {
            break;
        }

        // signaled가 false여도 (타임아웃) 재연결 시도
        // 이렇게 하면 서버 다운 시에도 무한 루프가 아닌 1초 간격 재시도
        int pending = pendingConnectCount_.load(std::memory_order_acquire);
        if (pending > 0)
        {
            LOG_INFO("Reconnecting %d failed connections", pending);

            for (int i = 0; i < pending; i++)
            {
                PostConnectEx();
            }

            pendingConnectCount_.fetch_sub(pending, std::memory_order_release);
        }
    }

    LOG_DEBUG("Reconnect thread stopped");
}

void Client::TriggerReconnect()
{
    pendingConnectCount_.fetch_add(1, std::memory_order_release);
    reconnectSignal_.release();  // 스레드 즉시 깨우기
}

void Client::OnUserDisconnect(User* user)
{
    LOG_INFO("User disconnected, triggering reconnect");

    // 재연결 트리거
    if (running_.load(std::memory_order_acquire))
    {
        TriggerReconnect();
    }

    delete user;
}

bool Client::LoadConnectExFunctions()
{
    // ConnectEx 함수 포인터 로딩
    GUID guidConnectEx = WSAID_CONNECTEX;
    DWORD dwBytes = 0;

    // 임시 소켓 생성 (함수 포인터 로딩용)
    SOCKET tempSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (tempSocket == INVALID_SOCKET)
    {
        LOG_ERROR("Failed to create temp socket for ConnectEx loading: %d", WSAGetLastError());
        return false;
    }

    int result = WSAIoctl(tempSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                         &guidConnectEx, sizeof(guidConnectEx),
                         &fnConnectEx, sizeof(fnConnectEx),
                         &dwBytes, NULL, NULL);

    closesocket(tempSocket);

    if (result == SOCKET_ERROR)
    {
        LOG_ERROR("Failed to load ConnectEx function pointer: %d", WSAGetLastError());
        return false;
    }

    return true;
}

bool Client::PostConnectEx()
{
    if (!fnConnectEx)
    {
        LOG_ERROR("ConnectEx function pointer not loaded");
        return false;
    }

    // 새 클라이언트 소켓 생성
    SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (clientSocket == INVALID_SOCKET)
    {
        LOG_ERROR("Failed to create client socket: %d", WSAGetLastError());
        return false;
    }

    // ConnectEx 사용 전 소켓을 로컬 주소에 바인딩 (필수)
    SOCKADDR_IN localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = 0; // 시스템이 자동으로 포트 할당

    if (bind(clientSocket, (SOCKADDR*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
    {
        LOG_ERROR("Failed to bind client socket: %d", WSAGetLastError());
        closesocket(clientSocket);
        return false;
    }

    // 클라이언트 소켓을 IOCP에 등록
    if (!iocpManager->RegisterSocket(clientSocket))
    {
        LOG_ERROR("Failed to register client socket to IOCP: %d", GetLastError());
        closesocket(clientSocket);
        return false;
    }

    // Session 사전 생성 및 설정
    auto session = std::make_shared<Session>();
    session->sock_ = clientSocket;
    session->SetEngine(this);

    // OverlappedEx 생성
    auto connectOverlapped = new OverlappedEx(session, IOOperation::IO_CONNECT);

    // 서버 주소 설정
    SOCKADDR_IN serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) != 1)
    {
        LOG_ERROR("Invalid server IP address: %s", serverIP.c_str());
        closesocket(clientSocket);
        delete connectOverlapped;
        return false;
    }

    // ConnectEx 호출
    DWORD bytesSent = 0;
    BOOL result = fnConnectEx(
        clientSocket,               // 클라이언트 소켓
        (SOCKADDR*)&serverAddr,     // 서버 주소
        sizeof(serverAddr),         // 주소 길이
        NULL,                       // 송신 데이터 버퍼 (없음)
        0,                          // 송신 데이터 길이 (0)
        &bytesSent,                 // 송신된 바이트 수
        &connectOverlapped->overlapped_ // OVERLAPPED 구조체
    );

    DWORD lastError = WSAGetLastError();
    if (!result && lastError != ERROR_IO_PENDING)
    {
        LOG_ERROR("ConnectEx failed: result=%d, error=%d", result, lastError);
        closesocket(clientSocket);
        delete connectOverlapped;
        return false;
    }

    LOG_DEBUG("Async connection initiated to %s:%d", serverIP.c_str(), serverPort);
    return true;
}
