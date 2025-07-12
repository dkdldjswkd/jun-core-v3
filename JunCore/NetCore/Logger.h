#pragma once

#include <string>
#include <memory>
#include <mutex>

// Simple logging interface
enum class LogLevel
{
	kDebug,
	kInfo,
	kWarning,
	kError
};

class ILogger
{
public:
	virtual ~ILogger() = default;
	virtual void Log(LogLevel level, const std::string& message) = 0;
};

// Default console logger implementation
class ConsoleLogger : public ILogger
{
public:
	void Log(LogLevel level, const std::string& message) override;
	void SetMinLevel(LogLevel min_level);

private:
	LogLevel min_level_ = LogLevel::kInfo;
	std::mutex log_mutex_;
};

// Global logger management
class Logger
{
public:
	static void SetLogger(std::shared_ptr<ILogger> logger);
	static void Log(LogLevel level, const std::string& message);
	static void Debug(const std::string& message);
	static void Info(const std::string& message);
	static void Warning(const std::string& message);
	static void Error(const std::string& message);

private:
	static std::shared_ptr<ILogger> logger_;
	static std::mutex logger_mutex_;
};