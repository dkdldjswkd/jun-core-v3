#pragma once
#include "../core/WindowsIncludes.h"

//=============================================================================
// DEPRECATED: 이 헤더들은 더 이상 사용하지 않습니다.
// UnifiedPacketHeader.h를 사용하세요.
//
// 마이그레이션:
// - LanHeader -> UnifiedPacketHeader (packet_id = LAN_PACKET_ID)
// - NetHeader -> UnifiedPacketHeader (packet_id = NET_PACKET_ID)
//=============================================================================

#pragma message("WARNING: LanHeader/NetHeader are DEPRECATED. Use UnifiedPacketHeader instead.")

#pragma pack(push, 1)
struct LanHeader 
{
	WORD len;
};

struct NetHeader 
{
	BYTE code;
	WORD len;
	BYTE randKey;
	BYTE checkSum;
};
#pragma pack(pop)

#define LAN_HEADER_SIZE  ((WORD)sizeof(LanHeader))
#define NET_HEADER_SIZE  ((WORD)sizeof(NetHeader))