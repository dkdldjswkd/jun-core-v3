#pragma once

#include <cstdint>
#include <cstddef>
#include "WindowsIncludes.h"

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

// 로그 매크로
// 추후 파일로그도 지원하게 변경예정

// 콘솔 색상 출력을 위한 ANSI 색상 코드
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Windows 콘솔에서 ANSI 색상 지원 활성화
inline void EnableConsoleColors() {
    static bool initialized = false;
    if (!initialized) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        initialized = true;
    }
}

#define LOG_ERROR(format, ...)   do { EnableConsoleColors(); printf(ANSI_COLOR_RED      "[ERROR] "  format ANSI_COLOR_RESET "\n", ##__VA_ARGS__); } while(0)
#define LOG_WARN(format, ...)    do { EnableConsoleColors(); printf(ANSI_COLOR_YELLOW   "[WARN] "   format ANSI_COLOR_RESET "\n", ##__VA_ARGS__); } while(0)  
#define LOG_INFO(format, ...)    do { EnableConsoleColors(); printf(ANSI_COLOR_CYAN     "[INFO] "   format ANSI_COLOR_RESET "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(format, ...)   do { EnableConsoleColors(); printf(ANSI_COLOR_GREEN    "[DEBUG] "  format ANSI_COLOR_RESET "\n", ##__VA_ARGS__); } while(0)

// 조건 검사 후 에러 로그와 함께 반환하는 매크로
#define LOG_ERROR_RETURN(condition, retval, format, ...)    \
    do {                                                    \
        if (!(condition)) {                                 \
            LOG_ERROR(format, ##__VA_ARGS__);               \
            return retval;                                  \
        }                                                   \
    } while(0)

// 조건 검사 후 에러 로그 출력하고 루프 계속 진행 (for/while에서 사용)
#define LOG_ERROR_CONTINUE(condition, format, ...)  \
    if (!(condition)) {                             \
        LOG_ERROR(format, ##__VA_ARGS__);           \
        continue;                                   \
    }

// 조건 검사 후 에러 로그 출력하고 void 함수에서 반환
#define LOG_ERROR_RETURN_VOID(condition, format, ...)   \
    do {                                                \
        if (!(condition)) {                             \
            LOG_ERROR(format, ##__VA_ARGS__);           \
            return;                                     \
        }                                               \
    } while(0)

// 조건 검사 후 경고 로그와 함께 반환하는 매크로
#define LOG_WARN_RETURN(condition, retval, format, ...) \
    do {                                                \
        if (!(condition)) {                             \
            LOG_WARN(format, ##__VA_ARGS__);            \
            return retval;                              \
        }                                               \
    } while(0)

// 조건 검사 후 경고 로그 출력하고 void 함수에서 반환
#define LOG_WARN_RETURN_VOID(condition, format, ...)    \
    do {                                                \
        if (!(condition)) {                             \
            LOG_WARN(format, ##__VA_ARGS__);            \
            return;                                     \
        }                                               \
    } while(0)

// 조건 검사 후 정보 로그와 함께 반환하는 매크로
#define LOG_INFO_RETURN(condition, retval, format, ...) \
    do {                                                \
        if (!(condition)) {                             \
            LOG_INFO(format, ##__VA_ARGS__);            \
            return retval;                              \
        }                                               \
    } while(0)

// 조건 검사 후 정보 로그 출력하고 void 함수에서 반환
#define LOG_INFO_RETURN_VOID(condition, format, ...)    \
    do {                                                \
        if (!(condition)) {                             \
            LOG_INFO(format, ##__VA_ARGS__);            \
            return;                                     \
        }                                               \
    } while(0)

// 조건 검사 후 디버그 로그와 함께 반환하는 매크로
#define LOG_DEBUG_RETURN(condition, retval, format, ...) \
    do {                                                 \
        if (!(condition)) {                              \
            LOG_DEBUG(format, ##__VA_ARGS__);            \
            return retval;                               \
        }                                                \
    } while(0)

// 조건 검사 후 디버그 로그 출력하고 void 함수에서 반환
#define LOG_DEBUG_RETURN_VOID(condition, format, ...)    \
    do {                                                 \
        if (!(condition)) {                              \
            LOG_DEBUG(format, ##__VA_ARGS__);            \
            return;                                      \
        }                                                \
    } while(0)
