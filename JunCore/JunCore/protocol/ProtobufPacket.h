#pragma once
#include "../core/base.h"
#include "../buffer/packet.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cstring>

// FNV-1a 해시 함수를 core/base.h에서 가져옴
// 패킷 ID 생성 매크로
#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

#pragma pack(push, 1)
// 간단한 패킷 헤더 (암호화 제외)
struct PacketLen
{
    uint32_t len_;  // 전체 패킷 크기 (헤더 + 페이로드)
};

struct PacketID
{
    uint32_t packet_id_; // 패킷 식별자 (protobuf 메시지 타입)
};

struct ProtobufHeader
{
    PacketLen len_;
    PacketID packet_id_;
};
#pragma pack(pop)

#define PROTOBUF_PACKET_LEN_SIZE    sizeof(PacketLen)
#define PROTOBUF_PACKET_ID_SIZE     sizeof(PacketID)
#define PROTOBUF_HEADER_SIZE        sizeof(ProtobufHeader)

//------------------------------
// Protobuf 패킷 핸들러 시스템
//------------------------------
class ProtobufPacketManager
{
public:
    // 패킷 핸들러 등록 (템플릿 기반)
    template<typename T>
    static void RegisterPacketHandler(std::function<void(const T&)> handler)
    {
        const auto packet_id = PACKET_ID(T);
        std::cout << "Registering packet handler for ID: " << packet_id 
                  << " (" << T::descriptor()->full_name() << ")" << std::endl;

        s_packet_handlers[packet_id] = [handler](const char* data, size_t size)
        {
            T message;
            if (message.ParseFromArray(data, size))
            {
                handler(message);
            }
            else
            {
                std::cout << "Failed to parse protobuf message" << std::endl;
            }
        };
    }

    // 패킷 처리
    //static bool ProcessPacket(const char* packet_data, size_t packet_size);

    // 패킷 직렬화 (PacketBuffer에 저장)
    template<typename T>
    static PacketBuffer* SerializePacket(const T& message)
    {
        // 직렬화된 데이터 크기 계산
        size_t payload_size = message.ByteSizeLong();
        size_t total_size = PROTOBUF_HEADER_SIZE + payload_size;

        // PacketBuffer 할당
        PacketBuffer* packet = PacketBuffer::Alloc();
        
        // 헤더 구성
        ProtobufHeader header;
        header.len_ = static_cast<uint32_t>(total_size);
        header.packet_id_ = PACKET_ID(T);

        // PacketBuffer에 헤더 추가
        packet->PutData(reinterpret_cast<const char*>(&header), sizeof(header));

        // PayLoad 직렬화 및 추가
        std::vector<char> payload_buffer(payload_size);
        message.SerializeToArray(payload_buffer.data(), payload_size);
        packet->PutData(payload_buffer.data(), payload_size);

        return packet;
    }

private:
    // 패킷 핸들러 저장소
    static std::unordered_map<uint32_t, std::function<void(const char*, size_t)>> s_packet_handlers;
};

//// 인라인 구현
//inline bool ProtobufPacketManager::ProcessPacket(const char* packet_data, size_t packet_size)
//{
//    if (packet_size < PROTOBUF_HEADER_SIZE) {
//        std::cout << "Packet too small: " << packet_size << " bytes" << std::endl;
//        return false;
//    }
//
//    // 헤더 파싱
//    const ProtobufHeader* header = reinterpret_cast<const ProtobufHeader*>(packet_data);
//    
//    if (header->len_ != packet_size) {
//        std::cout << "Packet size mismatch: header=" << header->len_ 
//                  << ", actual=" << packet_size << std::endl;
//        return false;
//    }
//
//    // 핸들러 찾기
//    auto it = s_packet_handlers.find(header->packet_id_);
//    if (it == s_packet_handlers.end()) {
//        std::cout << "No handler for packet ID: " << header->packet_id_ << std::endl;
//        return false;
//    }
//
//    // 페이로드 추출 및 핸들러 호출
//    const char* payload = packet_data + PROTOBUF_HEADER_SIZE;
//    size_t payload_size = packet_size - PROTOBUF_HEADER_SIZE;
//    
//    std::cout << "Processing packet ID: " << header->packet_id_ 
//              << ", payload size: " << payload_size << std::endl;
//
//    it->second(payload, payload_size);
//    return true;
//}

// Static 멤버 정의
inline std::unordered_map<uint32_t, std::function<void(const char*, size_t)>> 
    ProtobufPacketManager::s_packet_handlers;