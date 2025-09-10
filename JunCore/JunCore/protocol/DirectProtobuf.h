#pragma once
#include "../core/base.h"
#include "../network/Session.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cstring>

#pragma message("WARNING: DirectProtobuf.h is DEPRECATED. Use UnifiedPacketHeader.h instead.")

// 전역 패킷 핸들러 맵 - 여전히 사용 중
extern std::unordered_map<uint32_t, std::function<void(Session&, const std::vector<char>&)>> g_direct_packet_handler;

// 호환성을 위한 alias
#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

// 패킷 핸들러 등록
template<typename T>
void RegisterDirectPacketHandler(std::function<void(Session&, const T&)> _packet_handle)
{
    const auto _packet_id = PACKET_ID(T);
    LOG_DEBUG("Registering packet handler for ID: %d", _packet_id);

    g_direct_packet_handler[_packet_id] = [_packet_handle](Session& _session,const std::vector<char>& _serialized_packet)
    {
        T message;
        if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size()))
        {
            _packet_handle(_session, message);
        }
    };
}