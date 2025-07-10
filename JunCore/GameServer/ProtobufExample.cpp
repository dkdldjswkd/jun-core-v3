#include "../NetCore/ProtobufHelper.h"
#include "../NetCore/Protocol/GameMessage.pb.h"
#include <iostream>
#include <chrono>

using namespace juncore::protocol;

void ProtobufExample()
{
    std::cout << "=== Protobuf Example ===" << std::endl;
    
    // 1. 로그인 요청 메시지 생성
    LoginRequest login_req;
    login_req.set_username("testuser");
    login_req.set_password("password123");
    login_req.set_version("1.0.0");
    
    std::cout << "Created login request:" << std::endl;
    std::cout << "  Username: " << login_req.username() << std::endl;
    std::cout << "  Password: " << login_req.password() << std::endl;
    std::cout << "  Version: " << login_req.version() << std::endl;
    
    // 2. 메시지 직렬화
    std::vector<char> serialized_data;
    if (ProtobufHelper::SerializeToBytes(login_req, serialized_data)) {
        std::cout << "Login request serialized: " << serialized_data.size() << " bytes" << std::endl;
    }
    
    // 3. 메시지 역직렬화
    LoginRequest received_req;
    if (ProtobufHelper::ParseFromBytes(serialized_data, received_req)) {
        std::cout << "Received login request:" << std::endl;
        std::cout << "  Username: " << received_req.username() << std::endl;
        std::cout << "  Password: " << received_req.password() << std::endl;
        std::cout << "  Version: " << received_req.version() << std::endl;
    }
    
    // 4. JSON 변환 테스트
    std::string json;
    if (ProtobufHelper::MessageToJson(login_req, json)) {
        std::cout << "JSON representation: " << json << std::endl;
    }
    
    // 5. 게임 패킷 생성
    GamePacket game_packet;
    game_packet.mutable_header()->set_type(MessageType::LOGIN_REQUEST);
    game_packet.mutable_header()->set_size(static_cast<int32_t>(serialized_data.size()));
    
    // 현재 시간 설정
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    game_packet.mutable_header()->set_timestamp(timestamp);
    
    // oneof 필드 설정
    *game_packet.mutable_login_request() = login_req;
    
    std::cout << "Created game packet with header type: " << static_cast<int>(game_packet.header().type()) << std::endl;
    
    // 6. 네트워크 메시지 헬퍼 사용
    std::vector<char> network_data;
    if (NetworkMessageHelper::SerializeWithSizeHeader(game_packet, network_data)) {
        std::cout << "Network packet size: " << network_data.size() << " bytes" << std::endl;
        
        // 역직렬화
        GamePacket received_packet;
        if (NetworkMessageHelper::ParseWithSizeHeader(network_data.data(), network_data.size(), received_packet)) {
            std::cout << "Received packet type: " << static_cast<int>(received_packet.header().type()) << std::endl;
            
            if (received_packet.has_login_request()) {
                std::cout << "Contains login request for user: " << received_packet.login_request().username() << std::endl;
            }
        }
    }
    
    // 7. 채팅 메시지 예제
    ChatMessage chat_msg;
    chat_msg.set_player_id(123);
    chat_msg.set_message("Hello, World!");
    chat_msg.set_timestamp(timestamp);
    
    std::string chat_serialized;
    if (chat_msg.SerializeToString(&chat_serialized)) {
        std::cout << "Chat message serialized: " << chat_serialized.size() << " bytes" << std::endl;
        
        ChatMessage chat_received;
        if (chat_received.ParseFromString(chat_serialized)) {
            std::cout << "Chat from player " << chat_received.player_id() 
                      << ": " << chat_received.message() << std::endl;
        }
    }
    
    std::cout << "=== Example Complete ===" << std::endl;
}