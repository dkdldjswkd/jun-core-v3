#include "Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

//==============================================================================
// ConsoleLogger Implementation
//==============================================================================

void ConsoleLogger::Log(LogLevel level, const std::string& message)
{
	if (level < min_level_)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(log_mutex_);

	// Get current time
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::ostringstream oss;

	// Use localtime_s for safety
	std::tm local_tm;
	if (localtime_s(&local_tm, &time_t) == 0)
	{
		oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
	}
	else
	{
		oss << "TIME_ERROR";
	}

	oss << "." << std::setfill('0') << std::setw(3) << ms.count();

	std::string level_str;
	switch (level)
	{
	case LogLevel::kDebug:
		level_str = "[DEBUG]";
		break;
	case LogLevel::kInfo:
		level_str = "[INFO]";
		break;
	case LogLevel::kWarning:
		level_str = "[WARN]";
		break;
	case LogLevel::kError:
		level_str = "[ERROR]";
		break;
	}

	std::cout << oss.str() << " " << level_str << " " << message << std::endl;
}

void ConsoleLogger::SetMinLevel(LogLevel min_level)
{
	min_level_ = min_level;
}

//==============================================================================
// Logger Implementation
//==============================================================================

std::shared_ptr<ILogger> Logger::logger_ = std::make_shared<ConsoleLogger>();
std::mutex Logger::logger_mutex_;

void Logger::SetLogger(std::shared_ptr<ILogger> logger)
{
	std::lock_guard<std::mutex> lock(logger_mutex_);
	logger_ = logger ? logger : std::make_shared<ConsoleLogger>();
}

void Logger::Log(LogLevel level, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logger_mutex_);
	if (logger_)
	{
		logger_->Log(level, message);
	}
}

void Logger::Debug(const std::string& message)
{
	Log(LogLevel::kDebug, message);
}

void Logger::Info(const std::string& message)
{
	Log(LogLevel::kInfo, message);
}

void Logger::Warning(const std::string& message)
{
	Log(LogLevel::kWarning, message);
}

void Logger::Error(const std::string& message)
{
	Log(LogLevel::kError, message);
}