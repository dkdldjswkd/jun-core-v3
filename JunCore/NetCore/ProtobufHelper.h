#pragma once

#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include "Protocol/GameMessage.pb.h"
#include <string>
#include <vector>
#include <memory>

namespace juncore {
namespace protocol {

class ProtobufHelper
{
public:
    // 메시지 직렬화
    static bool SerializeToString(const google::protobuf::Message& message, std::string& output);
    static bool SerializeToBytes(const google::protobuf::Message& message, std::vector<char>& output);
    
    // 메시지 역직렬화
    static bool ParseFromString(const std::string& data, google::protobuf::Message& message);
    static bool ParseFromBytes(const std::vector<char>& data, google::protobuf::Message& message);
    static bool ParseFromBytes(const char* data, size_t size, google::protobuf::Message& message);
    
    // JSON 변환
    static bool MessageToJson(const google::protobuf::Message& message, std::string& json);
    static bool JsonToMessage(const std::string& json, google::protobuf::Message& message);
    
    // 유틸리티 함수
    static size_t GetSerializedSize(const google::protobuf::Message& message);
    static std::string GetMessageTypeName(const google::protobuf::Message& message);
    static bool ValidateMessage(const google::protobuf::Message& message);
};

// 네트워크 메시지 헬퍼 클래스
class NetworkMessageHelper
{
public:
    // 메시지 크기 헤더 포함한 직렬화 (4바이트 크기 헤더 + 데이터)
    static bool SerializeWithSizeHeader(const google::protobuf::Message& message, std::vector<char>& output);
    
    // 크기 헤더 포함한 역직렬화
    static bool ParseWithSizeHeader(const char* data, size_t total_size, google::protobuf::Message& message);
    
    // 메시지 크기 헤더 읽기
    static bool ReadSizeHeader(const char* data, size_t available_size, uint32_t& message_size);
    
    // 완전한 메시지 수신 확인
    static bool IsCompleteMessage(const char* data, size_t available_size);
    
private:
    static constexpr size_t SIZE_HEADER_LENGTH = 4;
};

} // namespace protocol
} // namespace juncore