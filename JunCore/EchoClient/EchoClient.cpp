#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoClient.h"
#include <iostream>

EchoClient::EchoClient() : NetBase()
{
}

EchoClient::~EchoClient() 
{
}

void EchoClient::Start()
{
    printf("EchoClient starting...\n");
    
    // 실제 서버 연결 시도
    if (Connect("127.0.0.1", 7777)) {
        printf("EchoClient connected successfully!\n");
    } else {
        printf("EchoClient connection failed!\n");
    }
}

void EchoClient::Stop()
{
    printf("EchoClient stopping...\n");
    connected = false;
}

void EchoClient::OnRecv(Session* session, PacketBuffer* packet)
{
    auto payloadSize = packet->GetPayloadSize();
    char* payloadData = new char[payloadSize + 1];
    payloadData[payloadSize] = '\0';

    packet->GetData(payloadData, payloadSize);
    std::cout << "[EchoClient] recv msg >> " << payloadData << std::endl;
    delete[] payloadData;
    
    packet->MoveRp(payloadSize);
    PacketBuffer::Free(packet);
}

void EchoClient::OnClientJoin(Session* session)
{
    printf("[EchoClient] Connected to server\n");
    connected = true;
    clientSession = session;
}

void EchoClient::OnClientLeave(Session* session)
{
    printf("[EchoClient] Disconnected from server\n");
    connected = false;
    clientSession = nullptr;
}

bool EchoClient::Connect(const char* serverIP, int port)
{
    printf("[EchoClient] Attempting to connect to %s:%d\n", serverIP, port);
    
    // 클라이언트 소켓 생성
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        printf("[EchoClient] Socket creation failed: %d\n", WSAGetLastError());
        return false;
    }
    
    // 서버 주소 설정
    SOCKADDR_IN serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    
    // 서버에 연결
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("[EchoClient] Connection failed: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        return false;
    }
    
    // 임시로 세션 객체 생성 (실제로는 IOCP Manager가 관리해야 함)
    if (!clientSession) {
        clientSession = new Session();
        clientSession->sock = clientSocket;
        clientSession->sessionId.sessionId = 1; // 임시 ID
        clientSession->SetEngine(this);
        connected = true;
        
        // OnClientJoin 호출
        OnClientJoin(clientSession);
    }
    
    return true;
}

bool EchoClient::SendMessage(const char* message)
{
    if (!connected || !clientSession) {
        printf("[EchoClient] Not connected or no session\n");
        return false;
    }

    // LAN 헤더 포함한 패킷 생성
    size_t messageLen = strlen(message);
    WORD headerLen = static_cast<WORD>(messageLen);
    
    // 직접 소켓으로 전송 (간단한 구현)
    char sendBuffer[8192];
    memcpy(sendBuffer, &headerLen, 2);  // LAN 헤더 (2바이트 길이)
    memcpy(sendBuffer + 2, message, messageLen);  // 실제 메시지
    
    int totalSize = 2 + static_cast<int>(messageLen);
    int sent = send(clientSession->sock, sendBuffer, totalSize, 0);
    
    if (sent == totalSize) {
        printf("[EchoClient] Sent successfully: %s (%d bytes with header)\n", message, totalSize);
        return true;
    } else {
        printf("[EchoClient] Send failed: %d\n", WSAGetLastError());
        return false;
    }
}