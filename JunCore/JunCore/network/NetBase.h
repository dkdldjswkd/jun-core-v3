#pragma once
#include "../core/WindowsIncludes.h"
#include "Session.h"
#include "../buffer/packet.h"
#include "IPacketHandler.h"
#include "IOCPManager.h"
#include <memory>

// NetBase - 순수 패킷 처리 핸들러
// 단일 책임: 완성된 패킷만 처리
class NetBase : public IPacketHandler
{
public:
    NetBase();
    virtual ~NetBase();

protected:
    // IOCP 매니저 참조
    std::shared_ptr<IOCPManager> iocpManager;

public:
    // IPacketHandler 인터페이스 구현
    void HandleCompletePacket(Session* session, PacketBuffer* packet) override final;
    void OnSessionConnected(Session* session) override;
    void OnSessionDisconnected(Session* session) override;
    void OnPacketProcessed() override;
    void OnSessionError(Session* session, const char* errorMsg) override;

protected:
    // 순수 가상 함수 - 파생 클래스에서 구현
    virtual void OnRecv(Session* session, PacketBuffer* packet) = 0;
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
    // 유틸리티 함수들
    bool SendPacket(Session* session, PacketBuffer* packet);
    void DisconnectSession(Session* session);

private:
    // 세션 유효성 검사
    bool IsSessionValid(Session* session) const {
        return session != nullptr && session->sock != INVALID_SOCKET;
    }
    
    // 패킷 유효성 검사
    bool IsPacketValid(PacketBuffer* packet) const {
        return packet != nullptr;
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

inline void NetBase::HandleCompletePacket(Session* session, PacketBuffer* packet)
{
    OnRecv(session, packet);
}

inline void NetBase::OnSessionConnected(Session* session)
{
    OnClientJoin(session);
}

inline void NetBase::OnSessionDisconnected(Session* session)
{
    OnClientLeave(session);
}

inline void NetBase::OnPacketProcessed()
{
    // 패킷 처리 완료 콜백
}

inline void NetBase::OnSessionError(Session* session, const char* errorMsg)
{
    OnError(session, errorMsg);
}

inline void NetBase::AttachIOCPManager(std::shared_ptr<IOCPManager> manager)
{
    iocpManager = manager;
    PacketHandlerRegistry::RegisterHandler(this);
}

inline void NetBase::DetachIOCPManager()
{
    if (iocpManager) {
        PacketHandlerRegistry::UnregisterHandler();
        iocpManager.reset();
    }
}

inline bool NetBase::IsIOCPManagerAttached() const noexcept
{
    return iocpManager != nullptr && iocpManager->IsValid();
}

inline bool NetBase::SendPacket(Session* session, PacketBuffer* packet)
{
    if (!IsSessionValid(session) || !IsPacketValid(packet)) {
        return false;
    }
    
    // 송신 큐에 패킷 추가
    session->sendQ.Enqueue(packet);
    
    // Send flag 체크 후 비동기 송신 시작
    if (InterlockedExchange8((char*)&session->sendFlag, true) == false) {
        // IOCPManager를 통한 송신
        if (iocpManager) {
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