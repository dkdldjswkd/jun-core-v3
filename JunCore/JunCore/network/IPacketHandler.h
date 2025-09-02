#pragma once
#include "Session.h"
#include "../buffer/packet.h"

//------------------------------
// IPacketHandler - 순수 패킷 처리 인터페이스
// 아름다운 추상화: "완성된 패킷만 처리한다"
//------------------------------
class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    //------------------------------
    // 핵심 인터페이스 - 완성된 패킷 처리
    //------------------------------
    virtual void HandleCompletePacket(Session* session, PacketBuffer* packet) = 0;
    
    //------------------------------
    // 세션 생명주기 이벤트
    //------------------------------
    virtual void OnSessionConnected(Session* session) = 0;
    virtual void OnSessionDisconnected(Session* session) = 0;
    
    //------------------------------
    // 선택적 인터페이스 - 모니터링 지원
    //------------------------------
    virtual void OnPacketProcessed() { /* 기본 구현: 아무것도 하지 않음 */ }
    virtual void OnSessionError(Session* session, const char* errorMsg) { /* 기본 구현: 아무것도 하지 않음 */ }

protected:
    //------------------------------
    // 유틸리티 함수들 - 파생 클래스에서 활용
    //------------------------------
    
    // 패킷 안전성 검사
    bool IsPacketValid(PacketBuffer* packet) const {
        return packet != nullptr && packet->GetPayloadSize() > 0;
    }
    
    // 세션 안전성 검사  
    bool IsSessionValid(Session* session) const {
        return session != nullptr && !session->disconnectFlag;
    }
};

//------------------------------
// PacketHandlerRegistry - 아름다운 등록 시스템 (향후 확장용)
//------------------------------
class PacketHandlerRegistry
{
private:
    static thread_local IPacketHandler* currentHandler;

public:
    // Thread-local 핸들러 등록/해제
    static void RegisterHandler(IPacketHandler* handler) {
        currentHandler = handler;
    }
    
    static void UnregisterHandler() {
        currentHandler = nullptr;
    }
    
    static IPacketHandler* GetCurrentHandler() {
        return currentHandler;
    }
    
    // 안전한 패킷 전달
    static bool DeliverPacket(Session* session, PacketBuffer* packet) {
        if (currentHandler) {
            // 패킷과 세션 유효성 검사는 public 메서드로 분리
            if (IsValidPacket(packet) && IsValidSession(session)) {
                currentHandler->HandleCompletePacket(session, packet);
                currentHandler->OnPacketProcessed();
                return true;
            }
        }
        return false;
    }

private:
    // 정적 검사 메서드들
    static bool IsValidPacket(PacketBuffer* packet) {
        return packet != nullptr && packet->GetPayloadSize() > 0;
    }
    
    static bool IsValidSession(Session* session) {
        return session != nullptr && !session->disconnectFlag;
    }
};

// Thread-local 정적 변수는 .cpp 파일에서 정의