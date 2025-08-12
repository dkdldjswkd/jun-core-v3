#include <array>
#include <functional>
#include <unordered_map>
#include <vector>
#include "game_message.pb.h"
#include <iostream>
#include <cstring>

#define OUT

struct Session;

// 단순 FNV-1a 32bit 해시 예제
constexpr uint32_t fnv1a(const char* s) {
	uint32_t hash = 2166136261u;
	while (*s) {
		hash ^= static_cast<uint8_t>(*s++);
		hash *= 16777619u;
	}
	return hash;
}

// 매크로로 패킷 ID 생성
#define PACKET_ID(T) fnv1a(T::descriptor()->full_name().c_str())

#pragma pack(push, 1)
struct NetHeader
{
	unsigned int len_;
	std::array<char, 8> iv_;
	unsigned int packet_id_;
};
#pragma pack(pop)

#define NET_HEADER_SIZE  ((int)sizeof(NetHeader))

template<typename T>
class PacketSerializer
{
public:
	NetHeader header_;
	T payload_; // google protobuf 사용

public:
	void Serialize(std::vector<char>& buffer)
	{
		// 페이로드 직렬화
		payload_.SerializeToArray(buffer.data() + NET_HEADER_SIZE, buffer.size() - NET_HEADER_SIZE);

		header_.len_ = NET_HEADER_SIZE + payload_.ByteSizeLong(); // 전체 패킷 길이
		header_.packet_id_ = PACKET_ID(T);

		// 헤더 직렬화
		std::memcpy(buffer.data(), &header_, NET_HEADER_SIZE);
	}
};

std::unordered_map<unsigned int, std::function<void(const std::vector<char>&/*serialized packet*/)>> g_packet_handler;

template<typename T>
void RegisterPacketHandler(std::function<void(const T&)> _packet_handle)
{
	g_packet_handler[PACKET_ID(T)] = [_packet_handle](const std::vector<char>& _serialized_packet)
	{
		T message;
		if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size()))
		{
			_packet_handle(message);
		}
	};
}

std::vector<char> recv_buffer; // 상대의 수신 버퍼라고 가정

// 코어단에서 지원해주는 Session
struct Session
{
	template<typename T>
	void send(const T& packet) 
	{
		PacketSerializer<T> serializer;
		serializer.payload_ = packet;
		
		std::vector<char> buffer(NET_HEADER_SIZE + packet.ByteSizeLong());
		serializer.Serialize(buffer);

		// 실제 send call 및 송신 성공으로 인해 상대의 수신 버퍼에 저장을 가정
		recv_buffer = buffer;
	}
};

Session session;

int packet_test()
{
	// 0. 서버 패킷 핸들러 등록
	{
		RegisterPacketHandler<game::Item>([](const game::Item& item)
			{
				std::cout << "Received Item: " << item.name() << ", Value: " << item.value() << std::endl;
			});
	}

	// 1. 클라이언트에서 패킷 송신
	{
		// 패킷 생성
		game::Item _item;
		_item.set_id(33);
		_item.set_name("test item");
		_item.set_value(100);

		// 패킷 송신
		session.send(_item);
	}

	// 2. 서버 측 패킷 조립
	{
		NetHeader header;

		// 헤더 카피 (recv된 데이터가 몇바이트인지 체크 등등 패킷 분리 작업 및 복호화 작업은 제외. 패킷 조립 조건 만족 및 복호화 가정)
		std::memcpy(&header, recv_buffer.data(), NET_HEADER_SIZE);

		// 페이로드 버퍼 마련
		std::vector<char> packet_data(header.len_ - NET_HEADER_SIZE);
		
		// 페이로드 추출
		std::memcpy(packet_data.data(), recv_buffer.data() + NET_HEADER_SIZE, packet_data.size());

		// 패킷 핸들러 호출
		g_packet_handler[header.packet_id_](packet_data);
	}
	
	return 0;
}