#pragma once
#include "../core/base.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cstring>

#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

#pragma pack(push, 1)
// JunCore LAN 모드 완전 호환 - 길이만 헤더에, 패킷ID는 페이로드에
struct DirectPacketHeader
{
    uint16_t len_;        // JunCore LAN 모드와 동일한 2바이트 길이
};

// 페이로드 시작 부분의 패킷 식별자
struct DirectPacketPayload
{
    uint32_t packet_id_;  // 패킷 식별자 (페이로드 첫 4바이트)
    // protobuf 데이터가 이어짐
};
#pragma pack(pop)

#define DIRECT_HEADER_SIZE      sizeof(DirectPacketHeader)
#define DIRECT_PAYLOAD_ID_SIZE  sizeof(DirectPacketPayload)

// 전역 패킷 핸들러 맵 (PacketTest.cpp와 동일)
extern std::unordered_map<uint32_t, std::function<void(const std::vector<char>&)>> g_direct_packet_handler;

// 패킷 핸들러 등록 (PacketTest.cpp와 동일)
template<typename T>
void RegisterDirectPacketHandler(std::function<void(const T&)> _packet_handle)
{
    const auto _packet_id = PACKET_ID(T);
    std::cout << "Registering direct packet handler for ID: " << _packet_id << std::endl;

    g_direct_packet_handler[_packet_id] = [_packet_handle](const std::vector<char>& _serialized_packet)
    {
        T message;
        if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size()))
        {
            _packet_handle(message);
        }
    };
}

// 패킷 직렬화 (JunCore LAN 모드 호환)
template<typename T>
std::vector<char> SerializeDirectPacket(const T& msg)
{
    // 크기 계산: 헤더(2바이트) + 패킷ID(4바이트) + protobuf데이터
    size_t protobuf_size = msg.ByteSizeLong();
    size_t total_packet_size = DIRECT_HEADER_SIZE + DIRECT_PAYLOAD_ID_SIZE + protobuf_size;

    // 버퍼 할당
    std::vector<char> packet_data(total_packet_size);

    // 헤더 설정 (LAN 모드: 2바이트 길이만)
    DirectPacketHeader* header = reinterpret_cast<DirectPacketHeader*>(packet_data.data());
    header->len_ = static_cast<uint16_t>(total_packet_size);

    // 페이로드 시작 부분에 패킷 ID 설정
    uint32_t* packet_id_ptr = reinterpret_cast<uint32_t*>(packet_data.data() + DIRECT_HEADER_SIZE);
    *packet_id_ptr = PACKET_ID(T);

    // Protobuf 데이터 직렬화
    msg.SerializeToArray(packet_data.data() + DIRECT_HEADER_SIZE + DIRECT_PAYLOAD_ID_SIZE, protobuf_size);

    std::cout << "Serialized packet: total_len=" << header->len_ 
              << ", packet_id=" << *packet_id_ptr 
              << ", protobuf_size=" << protobuf_size << std::endl;

    return packet_data;
}

// 패킷 처리 (JunCore LAN 모드 호환)
inline bool ProcessDirectPacket(const char* packet_data, size_t packet_size)
{
    if (packet_size < DIRECT_HEADER_SIZE + DIRECT_PAYLOAD_ID_SIZE) {
        std::cout << "Packet too small: " << packet_size << " bytes" << std::endl;
        return false;
    }

    // 헤더 파싱 (2바이트 길이만)
    const DirectPacketHeader* header = reinterpret_cast<const DirectPacketHeader*>(packet_data);
    
    std::cout << "Processing packet: header_len=" << header->len_ 
              << ", actual_size=" << packet_size << std::endl;

    // 페이로드에서 패킷 ID 추출
    const uint32_t* packet_id_ptr = reinterpret_cast<const uint32_t*>(packet_data + DIRECT_HEADER_SIZE);
    uint32_t packet_id = *packet_id_ptr;

    std::cout << "Extracted packet_id=" << packet_id << std::endl;

    // 핸들러 찾기
    auto it = g_direct_packet_handler.find(packet_id);
    if (it == g_direct_packet_handler.end()) {
        std::cout << "No handler for packet ID: " << packet_id << std::endl;
        return false;
    }

    // protobuf payload만 추출 (패킷ID 이후 부분)
    const char* protobuf_data = packet_data + DIRECT_HEADER_SIZE + DIRECT_PAYLOAD_ID_SIZE;
    size_t protobuf_size = packet_size - DIRECT_HEADER_SIZE - DIRECT_PAYLOAD_ID_SIZE;
    
    std::vector<char> protobuf_payload(protobuf_data, protobuf_data + protobuf_size);

    std::cout << "Calling handler with protobuf_size=" << protobuf_size << std::endl;

    // 패킷 핸들러 호출
    it->second(protobuf_payload);
    return true;
}