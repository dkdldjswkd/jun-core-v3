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