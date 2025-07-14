#include "PacketBuffer.h"
#include <cstring> // memcpy, memset 등을 위한 헤더

// 생성자: 초기 버퍼 할당 및 설정
PacketBuffer::PacketBuffer()
    : size_(HEADER_SIZE + MAX_PAYLOAD_LEN), encrypted_(false)
{
    // 동적 메모리 할당
    begin_ = new char[size_];
    end_ = begin_ + size_;

    // 초기 포인터 위치 설정
    payload_pos_ = begin_ + HEADER_SIZE;
    write_pos_ = begin_ + HEADER_SIZE;
}

// 소멸자: 동적 할당된 메모리 해제
PacketBuffer::~PacketBuffer()
{
    delete[] begin_;
}

// 패킷 버퍼 초기화
void PacketBuffer::Initialization()
{
    payload_pos_ = begin_ + HEADER_SIZE;
    write_pos_ = begin_ + HEADER_SIZE;
    encrypted_ = false;
}

// 패킷 시작 위치 반환
char* PacketBuffer::GetPacketPos() const
{
    return begin_;
}

// 버퍼가 비어있는지 확인
bool PacketBuffer::Empty() const {
    return write_pos_ <= payload_pos_;
}

// 버퍼가 가득 찼는지 확인
bool PacketBuffer::Full() const {
    return write_pos_ + 1 == end_;
}

// 남은 버퍼 공간 크기 반환
size_t PacketBuffer::GetFreeSize() const {
    return end_ - write_pos_;
}

// 전체 패킷 크기 반환
size_t PacketBuffer::GetPacketSize() const {
    return write_pos_ - begin_;
}

// 페이로드 크기 반환
size_t PacketBuffer::GetPayloadSize() const {
    return write_pos_ - payload_pos_;
}

// 현재 쓰기 위치 반환
char* PacketBuffer::GetWritePos() const
{
    return write_pos_;
}

// 패킷 헤더 설정 (암호화 준비)
void PacketBuffer::SetHeader(int32_t packet_id)
{
    // 이미 암호화된 패킷은 다시 처리하지 않음
    if (encrypted_) return;
    encrypted_ = true;

    // 패킷 헤더 설정
    PacketHeader* header = reinterpret_cast<PacketHeader*>(GetPacketPos());
    header->len = static_cast<uint16_t>(GetPayloadSize());
    header->pid = static_cast<uint16_t>(packet_id);

    // TODO: 실제 암호화 로직 구현 필요
}

// 패킷 복호화 메서드
bool PacketBuffer::DecryptPacket(char* encryptPacket, BYTE privateKey) {
    // 복호화 변수 초기화
    const BYTE RK = reinterpret_cast<PacketHeader*>(encryptPacket)->randKey;
    const BYTE K = privateKey;
    BYTE P = 0, LP = 0, LE = 0;

    // 복호화 위치 설정
    char* const packetPos = GetPacketPos();
    char* decryptPos = packetPos + (HEADER_SIZE - 1);
    char* encryptPos = encryptPacket + (HEADER_SIZE - 1);
    const uint16_t encryptLen = reinterpret_cast<PacketHeader*>(encryptPacket)->len + 1;

    // 헤더 부분 복사
    memcpy(packetPos, encryptPacket, HEADER_SIZE - 1);

    // 복호화 진행
    for (size_t i = 0; i < encryptLen; ++i) {
        P = (*encryptPos) ^ (LE + K + static_cast<BYTE>(i + 1));
        *reinterpret_cast<BYTE*>(decryptPos) = P ^ (LP + RK + static_cast<BYTE>(i + 1));

        // 다음 반복을 위한 변수 업데이트
        LE = *encryptPos;
        LP = P;
        encryptPos++;
        decryptPos++;
    }

    // 쓰기 포인터 이동
    MoveWp(encryptLen - 1);

    // 체크섬 검증
    return reinterpret_cast<PacketHeader*>(packetPos)->checkSum == GetChecksum();
}

// 체크섬 계산
BYTE PacketBuffer::GetChecksum() {
    uint16_t len = static_cast<uint16_t>(GetPayloadSize());
    uint32_t checkSum = 0;
    char* cpy_readPos = payload_pos_;

    for (size_t i = 0; i < len; i++) {
        checkSum += static_cast<BYTE>(*cpy_readPos);
        cpy_readPos++;
    }

    return static_cast<BYTE>(checkSum & 0xFF);
}

// 원시 데이터 삽입
void PacketBuffer::PutData(const char* src, size_t size) {
    if (write_pos_ + size <= end_) {
        memcpy(write_pos_, src, size);
        write_pos_ += size;
    }
    else {
        throw PacketException(PacketException::PUT_ERROR);
    }
}

// 원시 데이터 읽기
void PacketBuffer::GetData(char* dst, size_t size) {
    if (payload_pos_ + size <= write_pos_) {
        memcpy(dst, payload_pos_, size);
        payload_pos_ += size;
    }
    else {
        throw PacketException(PacketException::GET_ERROR);
    }
}