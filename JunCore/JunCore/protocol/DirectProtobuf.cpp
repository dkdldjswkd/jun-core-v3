#include "DirectProtobuf.h"

// 전역 패킷 핸들러 맵 정의 (PacketTest.cpp와 동일)
std::unordered_map<uint32_t, std::function<void(const std::vector<char>&)>> g_direct_packet_handler;