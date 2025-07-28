#pragma once
#include <Windows.h>

// 패킷 타입 정의 (단순화)
enum class PacketType : BYTE {
    HANDSHAKE_DATA = 0x01,      // 핸드셰이크 데이터 (상태에 따라 해석)
    ENCRYPTED_DATA = 0x10       // AES로 암호화된 일반 데이터
};

// 핸드셰이크 상태 정의
enum class HandshakeState : BYTE {
    INIT = 0,                   // 연결 직후 초기 상태
    WAITING_CLIENT_KEY,         // 서버: 클라이언트 RSA 공개키 대기
    WAITING_SERVER_KEY,         // 클라이언트: 서버 RSA 공개키 대기  
    WAITING_AES_KEY,            // 클라이언트: 서버에서 AES 키 대기
    WAITING_COMPLETE_ACK,       // 서버: 클라이언트 완료 확인 대기
    COMPLETED,                  // 핸드셰이크 완료, 암호화 통신 가능
    FAILED                      // 핸드셰이크 실패
};

#pragma pack(push, 1)

// 기본 패킷 헤더 (모든 패킷 공통)
struct BasePacketHeader {
    PacketType type;        // 패킷 타입 구분
    WORD       length;      // 전체 패킷 길이 (헤더 포함)
};

// 암호화되지 않은 패킷 헤더 (핸드셰이크용)
struct PlainPacketHeader : public BasePacketHeader {
    BYTE reserved[2];       // 향후 확장용 (4바이트 정렬)
};

// 암호화된 패킷 헤더 (일반 통신용)  
struct EncryptedPacketHeader : public BasePacketHeader {
    BYTE iv[16];           // AES 초기화 벡터 (16바이트)
    WORD originalLength;   // 암호화 전 원본 데이터 길이
};

#pragma pack(pop)

// 헤더 크기 상수
#define BASE_PACKET_HEADER_SIZE      ((WORD)sizeof(BasePacketHeader))
#define PLAIN_PACKET_HEADER_SIZE     ((WORD)sizeof(PlainPacketHeader))
#define ENCRYPTED_PACKET_HEADER_SIZE ((WORD)sizeof(EncryptedPacketHeader))

// 세션 상태 관리 구조체
struct HandshakeSession {
    HandshakeState state = HandshakeState::INIT;
    DWORD timeout_ms = 10000;           // 10초 타임아웃
    DWORD last_activity = 0;            // 마지막 활동 시간
    
    // 상태 전환 메서드
    bool CanTransitionTo(HandshakeState next_state) const;
    void TransitionTo(HandshakeState next_state);
    bool IsTimedOut() const;
};

// 패킷 타입 확인 유틸리티
inline bool IsHandshakePacket(PacketType type) {
    return (type == PacketType::HANDSHAKE_DATA);
}

inline bool IsEncryptedPacket(PacketType type) {
    return (type == PacketType::ENCRYPTED_DATA);
}

// 상태 전환 유효성 검사
inline bool HandshakeSession::CanTransitionTo(HandshakeState next_state) const {
    switch (state) {
        case HandshakeState::INIT:
            return (next_state == HandshakeState::WAITING_CLIENT_KEY);
        case HandshakeState::WAITING_CLIENT_KEY:
            return (next_state == HandshakeState::WAITING_SERVER_KEY);
        case HandshakeState::WAITING_SERVER_KEY:
            return (next_state == HandshakeState::WAITING_AES_KEY);
        case HandshakeState::WAITING_AES_KEY:
            return (next_state == HandshakeState::WAITING_COMPLETE_ACK);
        case HandshakeState::WAITING_COMPLETE_ACK:
            return (next_state == HandshakeState::COMPLETED);
        default:
            return (next_state == HandshakeState::FAILED);
    }
}

inline void HandshakeSession::TransitionTo(HandshakeState next_state) {
    if (CanTransitionTo(next_state)) {
        state = next_state;
        last_activity = GetTickCount();
    } else {
        state = HandshakeState::FAILED;
    }
}

inline bool HandshakeSession::IsTimedOut() const {
    return (GetTickCount() - last_activity) > timeout_ms;
}