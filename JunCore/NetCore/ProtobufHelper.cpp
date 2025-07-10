#include "ProtobufHelper.h"
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <cstring>
#include <algorithm>

namespace juncore {
namespace protocol {

bool ProtobufHelper::SerializeToString(const google::protobuf::Message& message, std::string& output)
{
    return message.SerializeToString(&output);
}

bool ProtobufHelper::SerializeToBytes(const google::protobuf::Message& message, std::vector<char>& output)
{
    std::string temp;
    if (!message.SerializeToString(&temp)) {
        return false;
    }
    
    output.assign(temp.begin(), temp.end());
    return true;
}

bool ProtobufHelper::ParseFromString(const std::string& data, google::protobuf::Message& message)
{
    return message.ParseFromString(data);
}

bool ProtobufHelper::ParseFromBytes(const std::vector<char>& data, google::protobuf::Message& message)
{
    return message.ParseFromArray(data.data(), static_cast<int>(data.size()));
}

bool ProtobufHelper::ParseFromBytes(const char* data, size_t size, google::protobuf::Message& message)
{
    return message.ParseFromArray(data, static_cast<int>(size));
}

bool ProtobufHelper::MessageToJson(const google::protobuf::Message& message, std::string& json)
{
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;
    
    auto status = google::protobuf::util::MessageToJsonString(message, &json, options);
    return status.ok();
}

bool ProtobufHelper::JsonToMessage(const std::string& json, google::protobuf::Message& message)
{
    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;
    
    auto status = google::protobuf::util::JsonStringToMessage(json, &message, options);
    return status.ok();
}

size_t ProtobufHelper::GetSerializedSize(const google::protobuf::Message& message)
{
    return message.ByteSizeLong();
}

std::string ProtobufHelper::GetMessageTypeName(const google::protobuf::Message& message)
{
    return "Unknown"; // Simplified implementation
}

bool ProtobufHelper::ValidateMessage(const google::protobuf::Message& message)
{
    return message.IsInitialized();
}

// NetworkMessageHelper 구현
bool NetworkMessageHelper::SerializeWithSizeHeader(const google::protobuf::Message& message, std::vector<char>& output)
{
    std::string serialized;
    if (!message.SerializeToString(&serialized)) {
        return false;
    }
    
    uint32_t size = static_cast<uint32_t>(serialized.size());
    output.resize(SIZE_HEADER_LENGTH + size);
    
    // 크기 헤더 쓰기 (little endian)
    std::memcpy(output.data(), &size, SIZE_HEADER_LENGTH);
    
    // 메시지 데이터 쓰기
    std::memcpy(output.data() + SIZE_HEADER_LENGTH, serialized.data(), size);
    
    return true;
}

bool NetworkMessageHelper::ParseWithSizeHeader(const char* data, size_t total_size, google::protobuf::Message& message)
{
    if (total_size < SIZE_HEADER_LENGTH) {
        return false;
    }
    
    uint32_t message_size;
    if (!ReadSizeHeader(data, total_size, message_size)) {
        return false;
    }
    
    if (total_size < SIZE_HEADER_LENGTH + message_size) {
        return false;
    }
    
    return message.ParseFromArray(data + SIZE_HEADER_LENGTH, static_cast<int>(message_size));
}

bool NetworkMessageHelper::ReadSizeHeader(const char* data, size_t available_size, uint32_t& message_size)
{
    if (available_size < SIZE_HEADER_LENGTH) {
        return false;
    }
    
    std::memcpy(&message_size, data, SIZE_HEADER_LENGTH);
    return true;
}

bool NetworkMessageHelper::IsCompleteMessage(const char* data, size_t available_size)
{
    if (available_size < SIZE_HEADER_LENGTH) {
        return false;
    }
    
    uint32_t message_size;
    if (!ReadSizeHeader(data, available_size, message_size)) {
        return false;
    }
    
    return available_size >= SIZE_HEADER_LENGTH + message_size;
}

} // namespace protocol
} // namespace juncore