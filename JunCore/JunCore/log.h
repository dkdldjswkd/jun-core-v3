#pragma once

#include <cassert>

// ���� ���Ϸα׵� �����ϵ��� ���� ����

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

inline void EnableConsoleColors()
{
	static bool initialized = false;
	if (!initialized)
	{
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

#define LOG_ERROR_RETURN(condition, retval, format, ...)    \
    do {                                                    \
        if (!(condition)) {                                 \
            LOG_ERROR(format, ##__VA_ARGS__);               \
            return retval;                                  \
        }                                                   \
    } while(0)

#define LOG_ERROR_CONTINUE(condition, format, ...)  \
    if (!(condition)) {                             \
        LOG_ERROR(format, ##__VA_ARGS__);           \
        continue;                                   \
    }

#define LOG_ERROR_RETURN_VOID(condition, format, ...)   \
    do {                                                \
        if (!(condition)) {                             \
            LOG_ERROR(format, ##__VA_ARGS__);           \
            return;                                     \
        }                                               \
    } while(0)

#define LOG_WARN_RETURN(condition, retval, format, ...) \
    do {                                                \
        if (!(condition)) {                             \
            LOG_WARN(format, ##__VA_ARGS__);            \
            return retval;                              \
        }                                               \
    } while(0)

#define LOG_WARN_RETURN_VOID(condition, format, ...)    \
    do {                                                \
        if (!(condition)) {                             \
            LOG_WARN(format, ##__VA_ARGS__);            \
            return;                                     \
        }                                               \
    } while(0)

#define LOG_INFO_RETURN(condition, retval, format, ...) \
    do {                                                \
        if (!(condition)) {                             \
            LOG_INFO(format, ##__VA_ARGS__);            \
            return retval;                              \
        }                                               \
    } while(0)

#define LOG_INFO_RETURN_VOID(condition, format, ...)    \
    do {                                                \
        if (!(condition)) {                             \
            LOG_INFO(format, ##__VA_ARGS__);            \
            return;                                     \
        }                                               \
    } while(0)

#define LOG_DEBUG_RETURN(condition, retval, format, ...) \
    do {                                                 \
        if (!(condition)) {                              \
            LOG_DEBUG(format, ##__VA_ARGS__);            \
            return retval;                               \
        }                                                \
    } while(0)

#define LOG_DEBUG_RETURN_VOID(condition, format, ...)    \
    do {                                                 \
        if (!(condition)) {                              \
            LOG_DEBUG(format, ##__VA_ARGS__);            \
            return;                                      \
        }                                                \
    } while(0)

#define LOG_ASSERT(format, ...)         \
    do {                                \
        LOG_ERROR(format, ##__VA_ARGS__); \
        assert(false);                  \
    } while(0)

#define LOG_ASSERT_RETURN(condition, retval, format, ...)  \
    do {                                                    \
        if (!(condition)) {                                 \
            LOG_ERROR(format, ##__VA_ARGS__);               \
            assert(condition);                              \
            return retval;                                  \
        }                                                   \
    } while(0)

#define LOG_ASSERT_RETURN_VOID(condition, format, ...)  \
    do {                                                \
        if (!(condition)) {                             \
            LOG_ERROR(format, ##__VA_ARGS__);           \
            assert(condition);                          \
            return;                                     \
        }                                               \
    } while(0)

#define LOG_ASSERT_CONTINUE(condition, format, ...)  \
    if (!(condition)) {                              \
        LOG_ERROR(format, ##__VA_ARGS__);            \
        assert(condition);                           \
        continue;                                    \
    }
