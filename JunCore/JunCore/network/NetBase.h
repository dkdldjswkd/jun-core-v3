#pragma once
#include "../core/WindowsIncludes.h"
#include "Session.h"
#include "IOCPManager.h"
#include <memory>
#include <iostream>

// NetBase - 네트워크 엔진 베이스 클래스
// 단일 책임: 패킷 처리와 세션 관리
class NetBase
{
public:
    NetBase();
    virtual ~NetBase();

protected:
    // IOCP 매니저 참조
    std::shared_ptr<IOCPManager> iocpManager;

public:
    virtual void OnClientJoin(Session* session) = 0;
    virtual void OnClientLeave(Session* session) = 0;

    // 선택적 가상 함수 - 파생 클래스에서 재정의 가능
    virtual void OnError(Session* session, const char* errorMsg) {
        printf("Session Error: %s\n", errorMsg);
    }

public:
    // IOCP 매니저 연결 인터페이스
    void AttachIOCPManager(std::shared_ptr<IOCPManager> manager);
    void DetachIOCPManager();
    bool IsIOCPManagerAttached() const noexcept;

    // 생명주기 관리
    virtual void Start() = 0;
    virtual void Stop() = 0;

protected:
    // 유틸리티 함수들 (PacketBuffer 완전 제거)
    bool SendRawData(Session* session, const char* data, size_t dataSize);
    bool SendRawData(Session* session, const std::vector<char>& data);
    void DisconnectSession(Session* session);

private:
    // 세션 유효성 검사
    bool IsSessionValid(Session* session) const {
        return session != nullptr && session->sock_ != INVALID_SOCKET;
    }
};

// 인라인 구현
inline NetBase::NetBase()
{
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (0 != wsaResult) 
    {
        printf("WSAStartup failed with error: %d\n", wsaResult);
        throw std::runtime_error("WSAStartup_ERROR");
    }
}

inline NetBase::~NetBase()
{
    DetachIOCPManager();
    WSACleanup();
}

inline void NetBase::AttachIOCPManager(std::shared_ptr<IOCPManager> manager)
{
    iocpManager = manager;
}

inline void NetBase::DetachIOCPManager()
{
    if (iocpManager) {
        iocpManager.reset();
    }
}

inline bool NetBase::IsIOCPManagerAttached() const noexcept
{
    return iocpManager != nullptr && iocpManager->IsValid();
}

inline bool NetBase::SendRawData(Session* session, const std::vector<char>& data)
{
    if (!IsSessionValid(session) || data.empty()) {
        return false;
    }
    
    std::cout << "[SEND_RAW] Queuing " << data.size() << " bytes for session " 
              << (session->session_id_.SESSION_UNIQUE & 0xFFFF) << std::endl;
    
    // vector<char>를 동적 할당해서 송신 큐에 추가
    std::vector<char>* packet_data = new std::vector<char>(data);
    session->send_q_.Enqueue(packet_data);
    
    // Send flag 체크 후 비동기 송신 시작
    if (InterlockedExchange8((char*)&session->send_flag_, true) == false) {
        // IOCPManager를 통한 송신
        if (iocpManager) 
        {
            iocpManager->PostAsyncSend(session);
        }
    }
    
    return true;
}

inline void NetBase::DisconnectSession(Session* session)
{
    if (IsSessionValid(session)) {
        session->DisconnectSession();
    }
}

inline bool NetBase::SendRawData(Session* session, const char* data, size_t dataSize)
{
    if (!IsSessionValid(session) || !data || dataSize == 0) {
        return false;
    }
    
    // const char*를 vector<char>로 변환 후 전송
    std::vector<char> packet_data(data, data + dataSize);
    return SendRawData(session, packet_data);
}