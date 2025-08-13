#include <array>
#include <functional>
#include <unordered_map>
#include <vector>
#include "game_message.pb.h"
#include <iostream>
#include <cstring>
#include <random>
#include "../JunCommon/crypto/AES128.h"

#define OUT

struct Session;

// 단순 FNV-1a 32bit 해시 예제
constexpr uint32_t fnv1a(const char* s)
{
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
// 헤더 중 암호화 되지않는 부분
struct NonCryptoHeader
{
	unsigned int len_;           // 평문 - 패킷 파싱용
	unsigned long long iv_;     // 평문 - 복호화용
};

// 헤더 중 암호화 되는 부분
struct CryptoHeader
{
	unsigned int packet_id_;     // 암호화됨
};

struct NetHeader
{
	NonCryptoHeader non_crypto_header_; // 암호화 되지 않는 헤더 (복호화 시 필요)
	CryptoHeader crypto_header_;		// 암호화 되는 헤더 
};
#pragma pack(pop)

#define NET_HEADER_SIZE  ((int)sizeof(NetHeader))
#define PLAIN_HEADER_SIZE  (sizeof(unsigned int) + 8)  // len + iv만

// PacketSerializer 제거 - Session::send에서 직접 처리

std::unordered_map<unsigned int, std::function<void(const std::vector<char>&/*serialized packet*/)>> g_packet_handler;

template<typename T>
void RegisterPacketHandler(std::function<void(const T&)> _packet_handle)
{
	const auto _packet_id = PACKET_ID(T);
	std::cout << "Registering packet handler for ID: " << _packet_id << std::endl;

	g_packet_handler[_packet_id] = [_packet_handle](const std::vector<char>& _serialized_packet)
		{
			T message;
			if (message.ParseFromArray(_serialized_packet.data(), _serialized_packet.size()))
			{
				_packet_handle(message);
			}
		};
}

std::vector<char> recv_buffer; // 복호화된 수신 버퍼
std::vector<char> encrypted_buffer; // 암호화된 네트워크 버퍼

// 코어단에서 지원해주는 Session
struct Session
{
private:
	std::vector<unsigned char> session_aes_key_;  // 세션별 AES128 키
	uint64_t send_seq_;                       // 송신 시퀀스 (IV로 사용)

public:
	Session()
	{
		// Session 생성 시 AES128 키 생성
		session_aes_key_ = AES128::GenerateRandomKey();
	}

	template<typename T>
	void send(const T& msg)
	{
		++send_seq_;
		unsigned long long _iv = send_seq_; // send_seq_를 iv로 사용

		// 크기 계산
		size_t payload_size = msg.ByteSizeLong();
		size_t crypto_size = sizeof(CryptoHeader) + payload_size;
		size_t encrypted_size = AES128::GetEncryptedSize(crypto_size);
		size_t total_packet_size = sizeof(NonCryptoHeader) + encrypted_size;

		// 버퍼 할당
		std::vector<char> packet_data(total_packet_size);
		std::vector<char> crypto_data(crypto_size);

		// CryptoHeader 설정
		CryptoHeader* crypto_header = reinterpret_cast<CryptoHeader*>(crypto_data.data());
		crypto_header->packet_id_ = PACKET_ID(T);

		// Payload 직렬화
		msg.SerializeToArray(crypto_data.data() + sizeof(CryptoHeader), payload_size);

		// 암호화
		bool encrypt_success = AES128::Encrypt(
			reinterpret_cast<const unsigned char*>(crypto_data.data()),
			crypto_data.size(),
			session_aes_key_.data(),
			reinterpret_cast<const unsigned char*>(&_iv),
			reinterpret_cast<unsigned char*>(packet_data.data() + sizeof(NonCryptoHeader)),
			encrypted_size
		);

		if (!encrypt_success) {
			std::cout << "암호화 실패!" << std::endl;
			return;
		}

		// NonCryptoHeader 설정
		NonCryptoHeader* header = reinterpret_cast<NonCryptoHeader*>(packet_data.data());
		header->len_ = total_packet_size;
		header->iv_ = _iv;

		// 네트워크 전송 (테스트용: encrypted_buffer에 저장)
		encrypted_buffer = packet_data;

		std::cout << "send complte (seq: " << send_seq_ << ")" << std::endl;
	}

	// 암호화된 패킷을 복호화해서 recv_buffer에 저장
	void decrypt_and_process()
	{
		if (encrypted_buffer.empty()) {
			std::cout << "암호화된 데이터가 없습니다!" << std::endl;
			return;
		}

		// NonCryptoHeader 읽기
		NetHeader* net_header = reinterpret_cast<NetHeader*>(encrypted_buffer.data());
		std::array<unsigned char, 16> iv;
		memcpy(iv.data(), &net_header->non_crypto_header_.iv_, sizeof(net_header->non_crypto_header_.iv_));

		std::cout << "수신 패킷 크기: " << net_header->non_crypto_header_.len_ << "Byte" << std::endl;
		std::cout << "IV : " << net_header->non_crypto_header_.iv_ << std::endl;

		// 암호화된 데이터 추출
		size_t encrypted_size = net_header->non_crypto_header_.len_ - sizeof(NonCryptoHeader);
		std::vector<unsigned char> ciphertext(encrypted_size);
		memcpy(ciphertext.data(), encrypted_buffer.data() + sizeof(NonCryptoHeader), encrypted_size);

		// 복호화된 데이터를 저장할 버퍼 준비
		std::vector<unsigned char> plaintext(ciphertext.size());
		size_t actual_plaintext_size = 0;

		// 복호화
		bool decrypt_success = AES128::Decrypt(
			ciphertext.data(),
			ciphertext.size(),
			session_aes_key_.data(),
			reinterpret_cast<const unsigned char*>(iv.data()),
			plaintext.data(),
			plaintext.size(),
			&actual_plaintext_size
		);

		if (!decrypt_success) {
			std::cout << "복호화 실패!" << std::endl;
			return;
		}

		// 5. 실제 크기에 맞게 버퍼 리사이즈
		plaintext.resize(actual_plaintext_size);

		// recv_buffer에 복호화된 데이터 저장
		recv_buffer.assign(plaintext.begin(), plaintext.end());
		std::cout << "복호화 완료, 크기: " << plaintext.size() << "B" << std::endl;
	}

	// 복호화에서 세션 키 접근을 위한 getter
	const std::vector<unsigned char>& GetSessionKey() const
	{
		return session_aes_key_;
	}
};

// 글로벌 send_seq 저장소 (테스트용)
uint64_t session_send_seq;

Session session;

int packet_test()
{
	// 0. 서버 패킷 핸들러 등록
	{
		RegisterPacketHandler<game::Item>([](const game::Item& item)
			{
				std::cout << "Item id    : " << item.id() << std::endl;
				std::cout << "Item name  : " << item.name() << std::endl;
				std::cout << "Item value : " << item.value() << std::endl;
			});
	}

	// 1. 클라이언트에서 패킷 송신
	{
		// 패킷 생성
		game::Item _item;

		// 패킷 세팅
		{
			int id;
			std::cout << "input item id >> ";
			std::cin >> id;
			_item.set_id(id);

			std::string name;
			std::cout << "input item name >> ";
			std::cin >> name;
			_item.set_name(name);

			int value;
			std::cout << "input item value >> ";
			std::cin >> value;
			_item.set_value(value);
		}

		// 패킷 송신
		session.send(_item);
	}

	// 2. 서버 측 패킷 처리
	{
		// 암호화된 패킷 복호화
		session.decrypt_and_process();

		// recv_buffer에서 CryptoHeader 추출
		if (recv_buffer.empty()) {
			std::cout << "복호화된 데이터가 없습니다!" << std::endl;
			return 0;
		}

		CryptoHeader crypto_header;
		std::memcpy(&crypto_header, recv_buffer.data(), sizeof(CryptoHeader));

		// payload만 추출
		std::vector<char> packet_data(recv_buffer.begin() + sizeof(CryptoHeader), recv_buffer.end());

		std::cout << "패킷 ID: " << crypto_header.packet_id_ << std::endl;

		// 패킷 핸들러 호출
		g_packet_handler[crypto_header.packet_id_](packet_data);
	}

	return 0;
}