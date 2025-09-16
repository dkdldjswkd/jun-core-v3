#pragma once
#include "../core/base.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>

#pragma pack(push, 1)
struct UnifiedPacketHeader
{
    uint32_t length;        // 전체 패킷 길이 (헤더 + 페이로드)
    uint32_t packet_id;     // 패킷 식별자 (protobuf name FNV-1a 해시)
};
#pragma pack(pop)

// 헤더 크기 상수
#define UNIFIED_HEADER_SIZE     ((uint32_t)sizeof(UnifiedPacketHeader))
#define MAX_PACKET_SIZE         (4 * 1024 * 1024)  // 4MB 최대 패킷 크기
#define MIN_PACKET_SIZE         UNIFIED_HEADER_SIZE

// 패킷 ID 생성 매크로 (Protobuf 메시지용)
#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

// 사용자 정의 패킷 ID 매크로 (일반 용도)
#define CUSTOM_PACKET_ID(str) fnv1a(str)

// 패킷 유효성 검사
inline bool IsValidPacketSize(uint32_t size)
{
    return size >= MIN_PACKET_SIZE && size <= MAX_PACKET_SIZE;
}

// 헤더 초기화 헬퍼
inline void InitializePacketHeader(UnifiedPacketHeader* header, uint32_t total_length, uint32_t id)
{
    header->length = total_length;
    header->packet_id = id;
}

//=============================================================================
// 기존 헤더들과의 호환성 매핑
//=============================================================================

// LanHeader 호환성 (길이만 사용)
#define LAN_PACKET_ID           CUSTOM_PACKET_ID("LAN_PACKET")

// NetHeader 호환성 (암호화 없는 기본 패킷)  
#define NET_PACKET_ID           CUSTOM_PACKET_ID("NET_PACKET")

// Protobuf 패킷은 PACKET_ID() 매크로 사용
// 예: PACKET_ID(echo::EchoRequest) -> 자동으로 FNV-1a 해시 생성

// 보안 패킷용 ID들
#define HANDSHAKE_PACKET_ID     CUSTOM_PACKET_ID("HANDSHAKE")
#define ENCRYPTED_PACKET_ID     CUSTOM_PACKET_ID("ENCRYPTED")

//=============================================================================
// 패킷 직렬화 유틸리티 함수들
//=============================================================================

// Protobuf 메시지를 vector<char>로 직렬화 (클라이언트용)
template<typename T>
std::vector<char> SerializeUnifiedPacket(const T& msg)
{
    // 크기 계산: 헤더(8바이트) + protobuf데이터
    size_t payload_size = msg.ByteSizeLong();
    size_t total_packet_size = UNIFIED_HEADER_SIZE + payload_size;

    // 버퍼 할당
    std::vector<char> packet_data(total_packet_size);

    // 헤더 설정 
    UnifiedPacketHeader* header = reinterpret_cast<UnifiedPacketHeader*>(packet_data.data());
    InitializePacketHeader(header, static_cast<uint32_t>(total_packet_size), PACKET_ID(T));

    // Protobuf 데이터 직렬화
    msg.SerializeToArray(packet_data.data() + UNIFIED_HEADER_SIZE, payload_size);

    LOG_DEBUG("Serialize Packet!! packet_id(%d), total_len(%d), payload_len(%d)", header->packet_id, header->length, payload_size);
    return packet_data;
}

// 패킷 처리 (서버/클라이언트 공통)
inline bool ProcessUnifiedPacket(const char* packet_data, size_t packet_size, 
                                 const std::unordered_map<uint32_t, std::function<void(const std::vector<char>&)>>& handlers)
{
    if (packet_size < UNIFIED_HEADER_SIZE) {
        LOG_ERROR("Packet too small: %zu bytes", packet_size);
        return false;
    }

    // 헤더 파싱
    const UnifiedPacketHeader* header = reinterpret_cast<const UnifiedPacketHeader*>(packet_data);
    
    if (!IsValidPacketSize(header->length) || header->length != packet_size) {
        LOG_ERROR("Invalid packet: header_len=%d, actual_size=%zu", header->length, packet_size);
        return false;
    }

    LOG_DEBUG("ProcessUnifiedPacket: len=%d, packet_id=%d", header->length, header->packet_id);

    // 핸들러 찾기
    auto it = handlers.find(header->packet_id);
    if (it == handlers.end()) {
        LOG_WARN("No handler for packet ID: %d", header->packet_id);
        return false;
    }

    // protobuf payload만 추출 (헤더 이후 부분)
    const char* protobuf_data = packet_data + UNIFIED_HEADER_SIZE;
    size_t protobuf_size = packet_size - UNIFIED_HEADER_SIZE;
    
    std::vector<char> protobuf_payload(protobuf_data, protobuf_data + protobuf_size);

    LOG_DEBUG("Calling handler with protobuf_size=%zu", protobuf_size);

    // 패킷 핸들러 호출
    it->second(protobuf_payload);
    return true;
}

//=============================================================================
// 마이그레이션 가이드
//=============================================================================
/*
기존 코드에서 새 헤더로 변경하는 방법:

1. LanHeader -> UnifiedPacketHeader
   Before: LanHeader header; header.len = size;
   After:  UnifiedPacketHeader header; 
           InitializePacketHeader(&header, size, LAN_PACKET_ID);

2. ProtobufHeader -> UnifiedPacketHeader  
   Before: ProtobufHeader header; header.len_ = size; header.packet_id_ = PACKET_ID(T);
   After:  UnifiedPacketHeader header;
           InitializePacketHeader(&header, size, PACKET_ID(T));

3. DirectProtobuf -> UnifiedPacketHeader
   Before: SerializeDirectPacket(message);
   After:  SerializeUnifiedPacket(message);

4. 패킷 파싱
   Before: const ProtobufHeader* header = (ProtobufHeader*)data;
           uint32_t id = header->packet_id_.packet_id_;
   After:  const UnifiedPacketHeader* header = (UnifiedPacketHeader*)data;
           uint32_t id = header->packet_id;
*/