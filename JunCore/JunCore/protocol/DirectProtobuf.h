#pragma once
#include "../core/base.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cstring>

//=============================================================================
// DEPRECATED: 이 헤더는 더 이상 사용하지 않습니다.
// UnifiedPacketHeader.h를 사용하세요.
//
// 마이그레이션:
// - SerializeDirectPacket() -> SerializeUnifiedPacket()  
// - ProcessDirectPacket() -> ProcessUnifiedPacket()
// - DirectPacketHeader -> UnifiedPacketHeader
//=============================================================================

#pragma message("WARNING: DirectProtobuf.h is DEPRECATED. Use UnifiedPacketHeader.h instead.")

// 모든 구조체와 매크로는 UnifiedPacketHeader.h로 이관되었습니다.
// 이 파일은 호환성을 위해서만 유지됩니다.

// 전역 패킷 핸들러 맵 - 여전히 사용 중
extern std::unordered_map<uint32_t, std::function<void(const std::vector<char>&)>> g_direct_packet_handler;

// 호환성을 위한 alias
#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

// 패킷 핸들러 등록 - UnifiedPacketHeader 방식으로 리다이렉트
template<typename T>
void RegisterDirectPacketHandler(std::function<void(const T&)> _packet_handle)
{
    const auto _packet_id = PACKET_ID(T);
    LOG_DEBUG("Registering packet handler for ID: %d", _packet_id);

    g_direct_packet_handler[_packet_id] = [_packet_handle](const std::vector<char>& _serialized_packet)
    {
        T message;
        if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size()))
        {
            _packet_handle(message);
        }
    };
}

// DEPRECATED: SerializeDirectPacket는 더 이상 사용하지 않습니다.
// SerializeUnifiedPacket을 사용하세요.

// DEPRECATED: ProcessDirectPacket는 더 이상 사용하지 않습니다.
// ProcessUnifiedPacket을 사용하세요.