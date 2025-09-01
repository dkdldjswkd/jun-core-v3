#pragma once
#include "../core/WindowsIncludes.h"
#include <memory>
#include <mutex>
#include <cstdlib>
#include "../protocol/message.h"
#include <exception>
#include "../../JunCommon/pool/LFObjectPoolTLS.h"

constexpr size_t MAX_PAYLOAD_LEN = 8000;
constexpr size_t MAX_HEADER_LEN = 10;

struct PacketException : public std::exception 
{
public:
	PacketException(const char* error_code, bool type) : exception(error_code), errorType(type) {}
	PacketException(bool type) : errorType(type) {}

public:
	bool errorType;
#define GET_ERROR 0 // >> Error
#define PUT_ERROR 1 // << Error
};

// NetworkEngine forward declarations
template<typename NetworkPolicy> class NetworkEngine;
struct ServerPolicy;
struct ClientPolicy;
class PacketBuffer 
{
private:
	PacketBuffer();
public:
	inline ~PacketBuffer();

private:
	template<typename NetworkPolicy> friend class NetworkEngine;
	friend LFObjectPoolTLS<PacketBuffer>;

private:
	static LFObjectPoolTLS<PacketBuffer> packetPool;

private:
	// �Ҵ� ����ȭ ����
	char* begin;
	char* end;
	const int bufSize;

	// �ܺ� read/write (����ȭ, ������ȭ)
	char* payloadPos;
	char* writePos;

	// ����
	int refCount;
	bool encrypted;

private:
	// ��Ŷ �ʱ�ȭ
	void Set();

	// ��� ����
	void SetLanHeader();
	void SetNetHeader(BYTE protocolCode, BYTE privateKey);

	public:
	// ��Ŷ ��ȣȭ (public으로 이동)
	bool DecryptPacket(char* encryptPacket, BYTE privateKey);
	// ��Ŷ ���� �ּ� ��ȯ (payLoadPos - headerLen)
	char* GetLanPacketPos();
	char* GetNetPacketPos();

	// ��Ŷ ������ ��ȯ (��Ŷ ��� ������ ����)
	inline int GetLanPacketSize();
	inline int GetNetPacketSize();

	private:
	// üũ�� ��ȯ
	BYTE GetChecksum();

public:
	// �Ҵ�, ��ȯ
	static PacketBuffer* Alloc();
	static void Free(PacketBuffer* instance);
	static void Free(PacketBuffer* instance, bool* isReleased);

	// Empty & Full
	inline bool Empty() const noexcept;
	inline bool Full() const noexcept;

	// Getter
	inline int GetFreeSize() const noexcept;
	inline int GetPayloadSize() const noexcept;

	// ref ����
	void IncrementRefCount();

	// Pool Count ��ȯ
	static int GetUseCount();

	// move pos
	inline void MoveWp(int size) { writePos += size; }
	inline void MoveRp(int size) { payloadPos += size; }

	// ����ȭ
	PacketBuffer& operator <<(const char& data);
	PacketBuffer& operator <<(const unsigned char& data);
	PacketBuffer& operator <<(const int& data);
	PacketBuffer& operator <<(const unsigned int& data);
	PacketBuffer& operator <<(const long& data);
	PacketBuffer& operator <<(const unsigned long& data);
	PacketBuffer& operator <<(const short& data);
	PacketBuffer& operator <<(const unsigned short& data);
	PacketBuffer& operator <<(const float& data);
	PacketBuffer& operator <<(const double& data);
	PacketBuffer& operator <<(const long long& data);
	PacketBuffer& operator <<(const unsigned long long& data);
	void PutData(const char* src, int size);

	// ������ȭ
	PacketBuffer& operator >>(char& data);
	PacketBuffer& operator >>(unsigned char& data);
	PacketBuffer& operator >>(int& data);
	PacketBuffer& operator >>(unsigned int& data);
	PacketBuffer& operator >>(long& data);
	PacketBuffer& operator >>(unsigned long& data);
	PacketBuffer& operator >>(short& data);
	PacketBuffer& operator >>(unsigned short& data);
	PacketBuffer& operator >>(float& data);
	PacketBuffer& operator >>(double& data);
	PacketBuffer& operator >>(long long& data);
	PacketBuffer& operator >>(unsigned long long& data);
	void GetData(char* dst, int size);
};

inline PacketBuffer::~PacketBuffer() {
	std::free((char*)begin);
}

inline int PacketBuffer::GetUseCount() {
	return packetPool.GetUseCount();
}

inline int PacketBuffer::GetLanPacketSize() {
	return static_cast<int>(writePos - payloadPos) + LAN_HEADER_SIZE;
}

inline int PacketBuffer::GetNetPacketSize() {
	return static_cast<int>(writePos - payloadPos) + NET_HEADER_SIZE;
}

inline void PacketBuffer::IncrementRefCount() {
	InterlockedIncrement((DWORD*)&refCount);
}

inline char* PacketBuffer::GetLanPacketPos() {
	return (payloadPos - LAN_HEADER_SIZE);
}

inline char* PacketBuffer::GetNetPacketPos() {
	return (payloadPos - NET_HEADER_SIZE);
}

inline bool PacketBuffer::Empty() const noexcept {
	if (writePos <= payloadPos) return true;
	return false;
}

inline bool PacketBuffer::Full() const noexcept {
	if (writePos + 1 == end) return true;
	return false;
}

inline int PacketBuffer::GetFreeSize() const noexcept {
	return static_cast<int>(end - writePos);
}

inline int PacketBuffer::GetPayloadSize() const noexcept {
	return static_cast<int>(writePos - payloadPos);
}