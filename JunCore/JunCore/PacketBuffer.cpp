#include "PacketBuffer.h"
#include <cstdlib>
#include <cstring>
#include <ctime>

LFObjectPoolTLS<PacketBuffer>  PacketBuffer::packetPool;

PacketBuffer::PacketBuffer() : bufSize(MAX_HEADER_LEN + MAX_PAYLOAD_LEN) 
{
	begin = (char*)std::malloc(bufSize);
	end = begin + bufSize;
	payloadPos = begin + MAX_HEADER_LEN;
	writePos = begin + MAX_HEADER_LEN;
}

void PacketBuffer::Set() 
{
	payloadPos = begin + MAX_HEADER_LEN;
	writePos = begin + MAX_HEADER_LEN;
	encrypted = false;
	refCount = 1;
}

PacketBuffer* PacketBuffer::Alloc() 
{
	PacketBuffer* p_packet = packetPool.Alloc();
	p_packet->Set();
	return p_packet;
}

void PacketBuffer::Free(PacketBuffer* instance) 
{
	if (0 == InterlockedDecrement((DWORD*)&instance->refCount))
	{
		packetPool.Free(instance);
	}
}

void PacketBuffer::Free(PacketBuffer* instance, bool* isReleased) 
{
	if (0 == InterlockedDecrement((DWORD*)&instance->refCount)) 
	{
		packetPool.Free(instance);
		*isReleased = true;
	}
	else 
	{
		*isReleased = false;
	}
}

void PacketBuffer::SetLanHeader() {
	LanHeader lanHeader;
	lanHeader.len = GetPayloadSize();
	std::memcpy(GetLanPacketPos(), &lanHeader, LAN_HEADER_SIZE);
}

void PacketBuffer::SetNetHeader(BYTE protocol_code, BYTE private_key) {
	// �ߺ� ��ȣȭ ���� �ʱ� ���� (�̹� ��ȣȭ �� ��Ŷ)
	if (encrypted) return;
	encrypted = true;

	NetHeader netHeader;
	netHeader.code = protocol_code;
	netHeader.len = GetPayloadSize();
	netHeader.randKey = (std::rand() & 0xFF);
	netHeader.checkSum = GetChecksum();
	std::memcpy(GetNetPacketPos(), &netHeader, NET_HEADER_SIZE);

	char* encryptPos = payloadPos - 1;		// ��ȣȭ'��' �ּ�
	short encrypt_len = netHeader.len + 1;	// ��ȣȭ�� ����
	BYTE RK = netHeader.randKey;			// ���� Ű
	BYTE K = private_key;					// ���� Ű
	BYTE P = 0, E = 0;						// ��ȣȭ ����

	// ��ȣȭ
	for (int i = 0; i < encrypt_len; i++, encryptPos++) {
		P = (*encryptPos) ^ (P + RK + (i + 1));
		E = P ^ (E + K + (i + 1));
		*((BYTE*)encryptPos) = E;
	}
}

// ��ȣ��Ŷ�� this�� ��ȣȭ
bool PacketBuffer::DecryptPacket(char* encryptPacket, BYTE privateKey) {
	// ��ȣȭ ����
	const BYTE RK = ((NetHeader*)encryptPacket)->randKey;
	const BYTE K = privateKey;
	BYTE P = 0, LP = 0, LE = 0;

	// ��ȣȭ �غ�
	char* const packetPos = GetNetPacketPos();						// this ��Ŷ �ּ�
	char* decryptPos = packetPos + (NET_HEADER_SIZE - 1);			// this ��ȣȭ'��' �ּ� (��ȣȭ �� : checksum + payload)
	char* encryptPos = encryptPacket + (NET_HEADER_SIZE - 1);		// ��ȣȭ ��
	const short encryptLen = ((NetHeader*)encryptPacket)->len + 1;	// ��ȣȭ �� ���� (checksum + payload)
	std::memcpy(packetPos, encryptPacket, NET_HEADER_SIZE - 1);			// ��ȣȭ �Ǿ����� ���� �κ� ����

	// ��ȣȭ
	for (int i = 0; i < encryptLen; ++i) { // ��ȣȭ
		// BYTE ���� ��ȣȭ
		P = (*encryptPos) ^ (LE + K + (i + 1));
		*((BYTE*)decryptPos) = P ^ (LP + RK + (i + 1));

		// ���� ���� �غ�
		LE = *encryptPos;
		LP = P;
		encryptPos++;
		decryptPos++;
	}
	MoveWp(encryptLen - 1); // ��ȣȭ �� ����� ���̷ε� �� ��ŭ move

	// ��ȣ��Ŷ�� �ŷڼ� ��� ��ȯ
	if (((NetHeader*)packetPos)->checkSum == GetChecksum()) 
	{
		return true;
	}
	return false;
}

BYTE PacketBuffer::GetChecksum() {
	WORD len = GetPayloadSize();

	DWORD checkSum = 0;
	char* cpy_readPos = payloadPos;
	for (int i = 0; i < len; i++) 
	{
		checkSum += *cpy_readPos;
		cpy_readPos++;
	}
	return (BYTE)(checkSum & 0xFF);
}

///////////////////////////////
//	operator <<
///////////////////////////////

PacketBuffer& PacketBuffer::operator<<(const char& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const unsigned char& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else 
	{
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const int& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else 
	{
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const unsigned int& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const long& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const unsigned long& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const short& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const unsigned short& data) 
{
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const float& data) {
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const double& data) {
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const long long& data) {
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator<<(const unsigned long long& data) {
	if (writePos + sizeof(data) <= end) {
		std::memcpy(writePos, &data, sizeof(data));
		writePos += sizeof(data);
	}
	else {
		throw PacketException(PUT_ERROR);
	}
	return *this;
}

///////////////////////////////
//	operator >>
///////////////////////////////

PacketBuffer& PacketBuffer::operator>>(char& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(unsigned char& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(int& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(unsigned int& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(long& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(unsigned long& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(short& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(unsigned short& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(float& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}

	return *this;
}

PacketBuffer& PacketBuffer::operator>>(double& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(long long& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

PacketBuffer& PacketBuffer::operator>>(unsigned long long& data) {
	if (payloadPos + sizeof(data) <= writePos) {
		std::memcpy(&data, payloadPos, sizeof(data));
		payloadPos += sizeof(data);
	}
	else {
		throw PacketException(GET_ERROR);
	}
	return *this;
}

///////////////////////////////
//	PUT, GET
///////////////////////////////

void PacketBuffer::PutData(const char* src, int size) {
	if (writePos + size <= end) {
		std::memcpy(writePos, src, size);
		writePos += size;
	}
	else {
		throw PacketException(PUT_ERROR);
	}
}

void PacketBuffer::GetData(char* dst, int size) {
	if (payloadPos + size <= writePos) {
		std::memcpy(dst, payloadPos, size);
		payloadPos += size;
	}
	else {
		throw PacketException(GET_ERROR);
	}
}