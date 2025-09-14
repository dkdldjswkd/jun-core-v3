#pragma once

#include <cstdint>
#include <cstddef>
#include "WindowsIncludes.h"
#include "../log.h"

// FNV-1a 32bit 해시 함수
constexpr uint32_t fnv1a(const char* s)
{
	uint32_t hash = 2166136261u;
	while (*s) {
		hash ^= static_cast<uint8_t>(*s++);
		hash *= 16777619u;
	}
	return hash;
}

// 바이트 배열용 FNV-1a 해시 함수
constexpr uint32_t fnv1a(const uint8_t* data, size_t len)
{
	uint32_t hash = 2166136261u;
	for (size_t i = 0; i < len; ++i) {
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash;
}

// FNV-1a 해쉬 알고리즘을 사용한 프로토콜 코드 생성
constexpr uint32_t make_protocol_code(uint8_t game_ver, uint8_t protocol_ver, uint8_t build_id) noexcept
{
	uint8_t data[3] = { game_ver, protocol_ver, build_id };
	return fnv1a(data, 3);
}

constexpr uint8_t GAME_VERSION		= 1;
constexpr uint8_t PROTOCOL_VERSION	= 1;
constexpr uint8_t BUILD_ID			= 1;

constexpr uint32_t PROTOCOL_CODE = make_protocol_code(GAME_VERSION, PROTOCOL_VERSION, BUILD_ID);