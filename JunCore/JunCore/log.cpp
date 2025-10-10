#include "log.h"
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>


Logger::Logger()
    : writeIndex(0)
    , readIndex(0)
    , shouldStop(false)
    , currentLogLevel(LOG_LEVEL_INFO)
    , policy_(LoggingPolicy::SYNC)
    , initialized_(false)
    , logFile(nullptr)
    , currentFileSize(0)
{
    // 기본 초기화만 수행 - 리소스 할당은 Initialize()에서
}

Logger::~Logger()
{
    if (initialized_ && policy_ == LoggingPolicy::ASYNC)
    {
        shouldStop = true;
        if (loggerThread.joinable())
        {
            loggerThread.join();
        }
    }
    
    if (logFile)
    {
        fclose(logFile);
        logFile = nullptr;
    }
}

Logger& Logger::GetInstance()
{
    static Logger* instance = new Logger();
    return *instance;
}

void Logger::Initialize(LogLevel level, LoggingPolicy policy)
{
    Logger& instance = GetInstance();
    instance.policy_ = policy;
    instance.currentLogLevel = level;
    
    // 로그 파일 생성 (공통)
    instance.OpenNewLogFile();
    
    // 비동기 정책인 경우에만 추가 리소스 할당
    if (policy == LoggingPolicy::ASYNC)
    {
        instance.InitializeAsyncResources();
    }
    
    instance.initialized_ = true;
}

void Logger::Shutdown()
{
    static bool shutdown_called = false;
    if (shutdown_called) return;
    shutdown_called = true;
    
    Logger& logger = GetInstance();
    
    if (logger.initialized_ && logger.policy_ == LoggingPolicy::ASYNC)
    {
        logger.shouldStop = true;
        if (logger.loggerThread.joinable())
        {
            logger.loggerThread.join();
        }
    }
    
    if (logger.logFile)
    {
        fclose(logger.logFile);
        logger.logFile = nullptr;
    }
}

void Logger::InitializeAsyncResources()
{
    // 링 버퍼 초기화
    for (int i = 0; i < RING_BUFFER_SIZE; ++i)
    {
        ringBuffer[i].valid = false;
        ringBuffer[i].text[0] = '\0';
    }
    
    // 원자 변수 초기화
    writeIndex.store(0);
    readIndex.store(0);
    shouldStop.store(false);
    
    // 로거 스레드 시작
    loggerThread = std::thread(&Logger::LoggerThreadFunc, this);
}

void Logger::Log(int level, const char* format, ...)
{
    if (!initialized_)
        return;
        
    // 로그 레벨 체크
    if (level > currentLogLevel)
        return;
    
    va_list args;
    va_start(args, format);
    
    if (policy_ == LoggingPolicy::SYNC)
    {
        // 동기 로깅
        LogSync(level, format, args);
    }
    else
    {
        // 비동기 로깅 (기존 로직)
        LogAsync(level, format, args);
    }
    
    va_end(args);
}

void Logger::LogAsync(int level, const char* format, va_list args)
{
    while (true)
    {
        uint32_t currentWrite = writeIndex.load();
        uint32_t currentRead = readIndex.load();
        
        // 버퍼 가득참 체크 (링 버퍼 인덱스로 비교)
        uint32_t nextWritePos = (currentWrite + 1) % RING_BUFFER_SIZE;
        uint32_t readPos = currentRead % RING_BUFFER_SIZE;
        
        if (nextWritePos == readPos)
        {
            return; // 버퍼 가득 참 - 로그 드랍
        }
        
        // CAS로 안전하게 writeIndex 증가
        if (writeIndex.compare_exchange_weak(currentWrite, currentWrite + 1))
        {
            // 성공적으로 슬롯 할당받음
            uint32_t ringIdx = currentWrite % RING_BUFFER_SIZE;
            
            LogMessage& msg = ringBuffer[ringIdx];
            msg.level = level;
            
            // 타임스탬프, 로그레벨, 스레드ID 추가
            auto now = std::time(nullptr);
            struct tm tm;
            localtime_s(&tm, &now);
            
            // 로그 레벨 문자열 변환
            const char* levelStr = "";
            switch (level)
            {
            case LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
            case LOG_LEVEL_WARN:  levelStr = "WARN";  break;
            case LOG_LEVEL_INFO:  levelStr = "INFO";  break;
            case LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
            default:              levelStr = "UNKNOWN"; break;
            }
            
            // 스레드 ID 가져오기
            DWORD threadId = GetCurrentThreadId();
            
            int prefixLen = snprintf(msg.text, MAX_LOG_LENGTH, "[%04d-%02d-%02d %02d:%02d:%02d][%s][%lu] ", 
                                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                                    levelStr, threadId);
            
            // 실제 로그 메시지 포맷팅
            vsnprintf(msg.text + prefixLen, MAX_LOG_LENGTH - prefixLen - 1, format, args);
            
            // 개행 문자 추가
            size_t len = strlen(msg.text);
            if (len < MAX_LOG_LENGTH - 2)
            {
                msg.text[len] = '\n';
                msg.text[len + 1] = '\0';
            }
            
            // valid 플래그는 마지막에 설정 (메모리 배리어 역할)
            msg.valid = true;
            break; // 성공적으로 완료
        }
    }
}

void Logger::LogSync(int level, const char* format, va_list args)
{
    char logText[MAX_LOG_LENGTH];
    
    // 타임스탬프, 로그레벨, 스레드ID 추가
    auto now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    
    // 로그 레벨 문자열 변환
    const char* levelStr = "";
    switch (level)
    {
    case LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
    case LOG_LEVEL_WARN:  levelStr = "WARN";  break;
    case LOG_LEVEL_INFO:  levelStr = "INFO";  break;
    case LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
    default:              levelStr = "UNKNOWN"; break;
    }
    
    // 스레드 ID 가져오기
    DWORD threadId = GetCurrentThreadId();
    
    int prefixLen = snprintf(logText, MAX_LOG_LENGTH, "[%04d-%02d-%02d %02d:%02d:%02d][%s][%lu] ", 
                            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec,
                            levelStr, threadId);
    
    // 실제 로그 메시지 포맷팅
    vsnprintf(logText + prefixLen, MAX_LOG_LENGTH - prefixLen - 1, format, args);
    
    // 개행 문자 추가
    size_t len = strlen(logText);
    if (len < MAX_LOG_LENGTH - 2)
    {
        logText[len] = '\n';
        logText[len + 1] = '\0';
    }
    
    // 직접 콘솔과 파일에 쓰기 (동기)
    WriteToConsole(level, logText);
    WriteToFile(logText);
}

void Logger::LoggerThreadFunc()
{
    while (!shouldStop)
    {
        bool processedAny = false;
        
        for (int i = 0; i < 100; ++i)
        {
            uint32_t currentReadIdx = readIndex.load();
            uint32_t ringIdx = currentReadIdx % RING_BUFFER_SIZE;
            
            LogMessage& msg = ringBuffer[ringIdx];
            
            if (msg.valid)
            {
                ProcessLogMessage(msg);
                msg.valid = false;
                readIndex.fetch_add(1);
                processedAny = true;
            }
            else
            {
                break;
            }
        }
        
        // 100ms 주기로 대기
        Sleep(100);
    }
}

void Logger::ProcessLogMessage(const LogMessage& msg)
{
    // 파일에는 모든 레벨 로그 기록
    WriteToFile(msg.text);
    
    // 콘솔에는 레벨 필터링 적용
    if (msg.level <= currentLogLevel)
    {
        WriteToConsole(msg.level, msg.text);
    }
}

void Logger::WriteToConsole(int level, const char* text)
{
    EnableConsoleColors();
    
    const char* color = "";
    
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        color = ANSI_COLOR_RED;
        break;
    case LOG_LEVEL_WARN:
        color = ANSI_COLOR_YELLOW;
        break;
    case LOG_LEVEL_INFO:
        color = ANSI_COLOR_CYAN;
        break;
    case LOG_LEVEL_DEBUG:
        color = ANSI_COLOR_GREEN;
        break;
    }
    
    printf("%s%s%s", color, text, ANSI_COLOR_RESET);
}

void Logger::WriteToFile(const char* text)
{
    if (!logFile)
        return;
    
    size_t textLen = strlen(text);
    
    // 파일 크기 체크 및 로테이션
    if (currentFileSize + textLen > MAX_FILE_SIZE)
    {
        fclose(logFile);
        logFile = nullptr;
        OpenNewLogFile();
    }
    
    if (logFile)
    {
        fwrite(text, 1, textLen, logFile);
        fflush(logFile); // 즉시 flush
        currentFileSize += textLen;
    }
}

bool Logger::OpenNewLogFile()
{
    std::string baseFileName = GenerateLogFileName(0);
    std::string fileName = baseFileName;
    
    // 기존 파일이 있고 10MB 미만이면 이어서 작성
    if (std::filesystem::exists(fileName))
    {
        auto fileSize = std::filesystem::file_size(fileName);
        if (fileSize < MAX_FILE_SIZE)
        {
            errno_t err = fopen_s(&logFile, fileName.c_str(), "a");
            if (err == 0 && logFile)
            {
                currentLogFileName = fileName;
                currentFileSize = fileSize;
                return true;
            }
        }
    }
    
    // 새 파일 시퀀스 찾기
    int sequence = 2;
    while (true)
    {
        fileName = GenerateLogFileName(sequence);
        
        if (!std::filesystem::exists(fileName))
        {
            // 새 파일 생성
            errno_t err = fopen_s(&logFile, fileName.c_str(), "w");
            if (err == 0) break;
        }
        
        // 기존 파일이 있고 여유 공간이 있으면 사용
        auto fileSize = std::filesystem::file_size(fileName);
        if (fileSize < MAX_FILE_SIZE)
        {
            errno_t err = fopen_s(&logFile, fileName.c_str(), "a");
            if (err == 0)
            {
                currentFileSize = fileSize;
                break;
            }
        }
        
        sequence++;
    }
    
    if (logFile)
    {
        currentLogFileName = fileName;
        if (sequence == 2) // 새 파일인 경우
        {
            currentFileSize = 0;
        }
        return true;
    }
    
    return false;
}

std::string Logger::GenerateLogFileName(int sequence)
{
    std::string processName = GetProcessName();
    std::string timeString = GetCurrentTimeString();
    
    if (sequence == 0)
    {
        return processName + "_" + timeString + ".log";
    }
    else
    {
        return processName + "_" + timeString + "_" + std::to_string(sequence) + ".log";
    }
}

std::string Logger::GetProcessName()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    
    std::string fullPath(buffer);
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos)
    {
        fullPath = fullPath.substr(lastSlash + 1);
    }
    
    size_t lastDot = fullPath.find_last_of(".");
    if (lastDot != std::string::npos)
    {
        fullPath = fullPath.substr(0, lastDot);
    }
    
    return fullPath;
}

std::string Logger::GetCurrentTimeString()
{
    auto now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday
        << std::setw(2) << tm.tm_hour
        << std::setw(2) << tm.tm_min
        << std::setw(2) << tm.tm_sec;
    
    return oss.str();
}

void Logger::EnableConsoleColors()
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