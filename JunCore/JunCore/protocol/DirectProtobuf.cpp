#include "DirectProtobuf.h"

std::unordered_map<uint32_t, std::function<void(Session&, const std::vector<char>&)>> g_direct_packet_handler;