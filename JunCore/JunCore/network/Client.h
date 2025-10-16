#pragma once
#include "NetBase.h"
#include "Session.h"
#include "User.h"
#include <atomic>
#include <string>
#include <memory>
#include <thread>
#include <semaphore>
#include <mswsock.h>

// ConnectEx 함수 포인터 타입 정의
typedef BOOL(WINAPI* LPFN_CONNECTEX)(
    SOCKET s,
    const struct sockaddr* name,
    int namelen,
    PVOID lpSendBuffer,
    DWORD dwSendDataLength,
    LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped
);

//------------------------------
// Client - 클라이언트 네트워크 엔진
// 자동 재연결 지원
//------------------------------
class Client : public NetBase
{
    friend class IOCPManager; // IOCPManager가 private 멤버에 접근할 수 있도록
public:
    Client(std::shared_ptr<IOCPManager> manager,
           const char* serverIP,
           WORD port,
           int targetConnectionCount = 1);
    virtual ~Client();

    // 복사/이동 금지
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

public:
    //------------------------------
    // Client 시작/정지 인터페이스
    //------------------------------
    void StartClient();
    void StopClient();

protected:
    //------------------------------
    // 클라이언트 전용 가상함수 - 사용자가 재정의
    //------------------------------
    virtual void OnConnectComplete(User* user, bool success) = 0;
    void OnUserDisconnect(User* user) override;

private:
    //------------------------------
    // 서버 연결 정보
    //------------------------------
    std::string serverIP;
    WORD serverPort;
    int targetConnectionCount_;

    // 재연결 관리
    std::atomic<int> pendingConnectCount_{0};
    std::atomic<bool> running_{false};
    std::thread reconnectThread_;
    std::counting_semaphore<1000> reconnectSignal_{0};  // counting_semaphore로 변경

    // ConnectEx 함수 포인터
    LPFN_CONNECTEX fnConnectEx = nullptr;

    // 내부 헬퍼 함수들
    void ReconnectThreadFunc();
    void TriggerReconnect();
    bool LoadConnectExFunctions();
    bool PostConnectEx();
};