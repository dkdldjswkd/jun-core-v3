#pragma once
#include "../core/WindowsIncludes.h"
#include "Session.h"
#include "../buffer/packet.h"
#include "IPacketHandler.h"
#include "IOCPManager.h"
#include "../../JunCommon/algorithm/Parser.h"
#include <memory>
#include <string>

//------------------------------
// NetBase - 순수 패킷 처리 핸들러로 재탄생
// 아름다운 단일 책임: "완성된 패킷만 처리한다"
//------------------------------
class NetBase : public IPacketHandler
{
public:
    NetBase(const char* systemFile, const char* configSection);
    virtual ~NetBase();

protected:
    // 설정 관련 (남겨둠)
    std::string configFile;
    std::string configSection;
    
    // 모니터링 관련 (순수 핸들러 성능 측정)
    DWORD packetProcessedTPS = 0;
    DWORD sessionConnectedCount = 0;
    alignas(64) DWORD packetProcessedCount = 0;
    alignas(64) DWORD sessionEventCount = 0;

    // IOCP 매니저 참조 (조합 패턴)
    std::shared_ptr<IOCPManager> iocpManager;

public:
    //------------------------------
    // IPacketHandler 인터페이스 구현
    //------------------------------
    void HandleCompletePacket(Session* session, PacketBuffer* packet) override final;
    void OnSessionConnected(Session* session) override;
    void OnSessionDisconnected(Session* session) override;
    void OnPacketProcessed() override;
    void OnSessionError(Session* session, const char* errorMsg) override;

protected:
    //------------------------------
    // 순수 가상 함수 - 파생 클래스에서 구현
    //------------------------------
    virtual void OnRecv(Session* session, PacketBuffer* packet) = 0;
    virtual void OnClientJoin(Session* session) = 0;
    virtual void OnClientLeave(Session* session) = 0;

    //------------------------------
    // 선택적 가상 함수 - 파생 클래스에서 재정의 가능
    //------------------------------
    virtual void OnError(Session* session, const char* errorMsg) {
        // 기본 구현: 로그 출력 (향후 Logger 연동)
        printf("Session Error: %s\n", errorMsg);
    }

public:
    //------------------------------
    // IOCP 매니저 연결 인터페이스
    //------------------------------
    void AttachIOCPManager(std::shared_ptr<IOCPManager> manager);
    void DetachIOCPManager();
    bool IsIOCPManagerAttached() const noexcept;

    //------------------------------
    // 생명주기 관리
    //------------------------------
    virtual void Start() = 0;  // 순수 가상 함수
    virtual void Stop() = 0;   // 순수 가상 함수

public:
    //------------------------------
    // 모니터링 인터페이스 - 아름다운 추상화
    //------------------------------
    void UpdateTPS();
    DWORD GetPacketTPS() const noexcept { return packetProcessedTPS; }
    DWORD GetSessionCount() const noexcept { return sessionConnectedCount; }

protected:
    //------------------------------
    // 유틸리티 함수들
    //------------------------------
    
    // 안전한 패킷 전송
    bool SendPacket(Session* session, PacketBuffer* packet);
    
    // 세션 연결 해제 요청
    void DisconnectSession(Session* session);

private:
    //------------------------------
    // 내부 구현
    //------------------------------
    void InitializeFromConfig(const char* systemFile, const char* configSection);
    
    // TPS 계산을 위한 내부 카운터 업데이트
    void IncrementPacketCount() noexcept {
        InterlockedIncrement(&packetProcessedCount);
    }
    
    void IncrementSessionEvent() noexcept {
        InterlockedIncrement(&sessionEventCount);
    }
};

//------------------------------
// 인라인 구현
//------------------------------

inline NetBase::NetBase(const char* systemFile, const char* configSection)
    : configFile(systemFile), configSection(configSection)
{
    // WSAStartup은 IOCPManager가 담당하도록 변경 예정
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (0 != wsaResult) 
    {
        printf("!!! WSAStartup failed with error: %d !!!\n", wsaResult);
        throw std::runtime_error("WSAStartup_ERROR");
    }
    
    InitializeFromConfig(systemFile, configSection);
}

inline NetBase::~NetBase()
{
    DetachIOCPManager();
    WSACleanup();
}

inline void NetBase::HandleCompletePacket(Session* session, PacketBuffer* packet)
{
    // 📦 완성된 패킷을 받아서 사용자 핸들러로 전달
    OnRecv(session, packet);
    IncrementPacketCount();
}

inline void NetBase::OnSessionConnected(Session* session)
{
    OnClientJoin(session);
    InterlockedIncrement(&sessionConnectedCount);
    IncrementSessionEvent();
}

inline void NetBase::OnSessionDisconnected(Session* session)
{
    OnClientLeave(session);
    InterlockedDecrement(&sessionConnectedCount);
    IncrementSessionEvent();
}

inline void NetBase::OnPacketProcessed()
{
    // 패킷 처리 완료 시 호출되는 콜백
    // 향후 성능 모니터링이나 로깅에 활용
}

inline void NetBase::OnSessionError(Session* session, const char* errorMsg)
{
    OnError(session, errorMsg);
    IncrementSessionEvent();
}

inline void NetBase::AttachIOCPManager(std::shared_ptr<IOCPManager> manager)
{
    iocpManager = manager;
    
    // Thread-local 핸들러 등록 (현재 스레드에서)
    PacketHandlerRegistry::RegisterHandler(this);
}

inline void NetBase::DetachIOCPManager()
{
    if (iocpManager) {
        // Thread-local 핸들러 해제
        PacketHandlerRegistry::UnregisterHandler();
        iocpManager.reset();
    }
}

inline bool NetBase::IsIOCPManagerAttached() const noexcept
{
    return iocpManager != nullptr && iocpManager->IsValid();
}

inline void NetBase::UpdateTPS()
{
    packetProcessedTPS = packetProcessedCount;
    packetProcessedCount = 0;
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
        // IOCPManager를 통한 송신 (향후 구현)
        // iocpManager->PostAsyncSend(session);
    }
    
    return true;
}

inline void NetBase::DisconnectSession(Session* session)
{
    if (IsSessionValid(session)) {
        session->DisconnectSession();
    }
}