#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <cstdint>
#include <cstring> // memcpy를 위해 추가

// 플랫폼 독립적인 바이트 타입 정의
#ifndef BYTE
using BYTE = uint8_t;
#endif

// 최대 페이로드 길이 (플랫폼 독립적)
constexpr size_t MAX_PAYLOAD_LEN = 8000;

// 메모리 레이아웃 일관성을 위한 패킹된 구조체
#pragma pack(push, 1)
struct PacketHeader {
    BYTE code;           // 프로토콜 코드
    uint16_t len;        // 패킷 길이
    BYTE randKey;        // 암호화 랜덤 키
    BYTE checkSum;       // 체크섬
    uint16_t pid;        // 패킷 ID
};
#pragma pack(pop)

// 컴파일 타임 헤더 크기 상수
constexpr size_t HEADER_SIZE = sizeof(PacketHeader);

// 사용자 정의 예외 클래스
class PacketException : public std::runtime_error {
public:
    // 오류 유형 열거
    enum ErrorType {
        GET_ERROR = 0,   // 데이터 읽기 오류
        PUT_ERROR = 1    // 데이터 쓰기 오류
    };

    // 생성자
    PacketException(ErrorType type, const std::string& message = "")
        : std::runtime_error(message), errorType(type) {}

    // 오류 유형 반환
    ErrorType getErrorType() const { return errorType; }

private:
    ErrorType errorType;
};

class PacketBuffer {
public:
    PacketBuffer();     // 생성자
    ~PacketBuffer();    // 소멸자

    // 복사 및 이동 생성자/대입 연산자 삭제 (복사 방지)
    PacketBuffer(const PacketBuffer&) = delete;
    PacketBuffer& operator=(const PacketBuffer&) = delete;
    PacketBuffer(PacketBuffer&&) = delete;
    PacketBuffer& operator=(PacketBuffer&&) = delete;

private:
    // 버퍼 관리 멤버 변수
    char* begin_;       // 버퍼 시작 포인터
    char* end_;         // 버퍼 끝 포인터
    size_t size_;       // 버퍼 크기
    char* payload_pos_; // 페이로드 시작 포인터
    char* write_pos_;   // 쓰기 위치 포인터
    bool encrypted_;    // 암호화 상태

private:
    // 패킷 복호화 메서드
    bool DecryptPacket(char* encryptPacket, BYTE privateKey);

    // 체크섬 계산 메서드
    BYTE GetChecksum();

public:
    // 패킷 초기화
    void Initialization();

    // 패킷 헤더 설정
    void SetHeader(int32_t packet_id);

    // 버퍼 상태 확인
    bool Empty() const;  // 버퍼가 비어있는지 확인
    bool Full() const;   // 버퍼가 꽉 찼는지 확인

    // 각종 정보 접근자
    char* GetPacketPos() const;     // 패킷 시작 위치 반환
    char* GetWritePos() const;      // 쓰기 위치 반환
    size_t GetFreeSize() const;     // 남은 공간 크기 반환
    size_t GetPacketSize() const;   // 전체 패킷 크기 반환
    size_t GetPayloadSize() const;  // 페이로드 크기 반환

    // 포인터 위치 이동
    void MoveWp(size_t size) { write_pos_ += size; }
    void MoveRp(size_t size) { payload_pos_ += size; }

    // 템플릿 직렬화 연산자 (다양한 타입 처리)
    template<typename T>
    PacketBuffer& operator<<(const T& data) {
        // 버퍼 오버플로우 체크
        if (write_pos_ + sizeof(T) <= end_) {
            memcpy(write_pos_, &data, sizeof(T));
            write_pos_ += sizeof(T);
        }
        else {
            throw PacketException(PacketException::PUT_ERROR, "버퍼 공간 부족");
        }
        return *this;
    }

    // 템플릿 역직렬화 연산자
    template<typename T>
    PacketBuffer& operator>>(T& data) {
        // 데이터 부족 체크
        if (payload_pos_ + sizeof(T) <= write_pos_) {
            memcpy(&data, payload_pos_, sizeof(T));
            payload_pos_ += sizeof(T);
        }
        else {
            throw PacketException(PacketException::GET_ERROR, "읽을 데이터 부족");
        }
        return *this;
    }

    // 원시 데이터 메서드
    void PutData(const char* src, size_t size);
    void GetData(char* dst, size_t size);
};

// PacketBuffer에 대한 스마트 포인터 타입 정의
using PacketBufferPtr = std::shared_ptr<PacketBuffer>;