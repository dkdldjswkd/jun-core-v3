#pragma once

#include <cstdint>

// FNV-1a 해쉬 알고리즘
constexpr uint32_t make_protocol_code(uint8_t game_ver, uint8_t protocol_ver, uint8_t build_id) noexcept
{
	constexpr uint32_t FNV_OFFSET_BASIS = 2166136261u;
	constexpr uint32_t FNV_PRIME = 16777619u;

	uint32_t hash = FNV_OFFSET_BASIS;
	hash ^= game_ver;
	hash *= FNV_PRIME;
	hash ^= protocol_ver;
	hash *= FNV_PRIME;
	hash ^= build_id;
	hash *= FNV_PRIME;
	
	return hash;
}

constexpr uint8_t GAME_VERSION		= 1;
constexpr uint8_t PROTOCOL_VERSION	= 1;
constexpr uint8_t BUILD_ID			= 1;

constexpr uint32_t PROTOCOL_CODE = make_protocol_code(GAME_VERSION, PROTOCOL_VERSION, BUILD_ID);