#pragma once

#include <cassert>
#include <windows.h>
#include <thread>
#include <string>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <atomic>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

enum LogLevel
{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

enum class LoggingPolicy
{
    SYNC,   // 동기 로깅 (직접 쓰기)
    ASYNC   // 비동기 로깅 (링버퍼 + 스레드)
};

class Logger
{
private:
    static constexpr int RING_BUFFER_SIZE = 4096;
    static constexpr int MAX_LOG_LENGTH = 1024;
    static constexpr long long MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB

    struct LogMessage
    {
        char text[MAX_LOG_LENGTH];
        int level;
        volatile bool valid;
        
        LogMessage() : level(0), valid(false) 
        {
            text[0] = '\0';
        }
    };

    LogMessage ringBuffer[RING_BUFFER_SIZE];
    std::atomic<uint32_t> writeIndex;
    std::atomic<uint32_t> readIndex;
    std::atomic<bool> shouldStop;
    
    LogLevel currentLogLevel;
    LoggingPolicy policy_;
    bool initialized_;
    std::thread loggerThread;
    
    FILE* logFile;
    std::string currentLogFileName;
    long long currentFileSize;
    

private:
    Logger();
    ~Logger();
    
    void LoggerThreadFunc();
    void ProcessLogMessage(const LogMessage& msg);
    void WriteToConsole(int level, const char* text);
    void WriteToFile(const char* text);
    bool OpenNewLogFile();
    std::string GenerateLogFileName(int sequence = 0);
    std::string GetProcessName();
    std::string GetCurrentTimeString();
    void EnableConsoleColors();
    void LogSync(int level, const char* format, va_list args);
    void LogAsync(int level, const char* format, va_list args);
    void InitializeAsyncResources();

public:
    static Logger& GetInstance();
    static void Initialize(LogLevel level = LOG_LEVEL_INFO, LoggingPolicy policy = LoggingPolicy::SYNC);
    static void Shutdown();
    
    void SetLogLevel(LogLevel level) { currentLogLevel = level; }
    LogLevel GetLogLevel() const { return currentLogLevel; }
    
    void Log(int level, const char* format, ...);
};

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

#define LOGGER_INITIALIZE(level, policy) Logger::Initialize(level, policy)
#define LOGGER_INITIALIZE_SYNC(level) Logger::Initialize(level, LoggingPolicy::SYNC)
#define LOGGER_INITIALIZE_ASYNC(level) Logger::Initialize(level, LoggingPolicy::ASYNC)
#define LOGGER_SHUTDOWN() Logger::Shutdown()

#define SET_LOG_LEVEL(level) Logger::GetInstance().SetLogLevel(level)

#define LOG_ERROR(format, ...)   Logger::GetInstance().Log(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)    Logger::GetInstance().Log(LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)    Logger::GetInstance().Log(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...)   Logger::GetInstance().Log(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

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