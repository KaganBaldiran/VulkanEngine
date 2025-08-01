#pragma once
#include <queue>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace VKCOMMON
{
	enum LogSeverity
	{
		LOG_SEVERITY_INFO = 0,
		LOG_SEVERITY_WARNING = 1,
		LOG_SEVERITY_ERROR = 2,
		LOG_SEVERITY_VERBOSE = 3
	};

	struct LogMessage
	{
		LogSeverity Severity;
		std::string Message;
	};

	class Logger
	{
	public:
		Logger();
		~Logger();

		static Logger& Get();

		void FileLog(const char* FilePath,const LogMessage& Message);
		void ConsoleLog(const LogMessage& Message);
	private:
		void FileFlush();

		std::mutex Mutex;
		std::unordered_map<std::string, std::ofstream> Files;
		std::vector<std::pair<LogMessage,std::string>> MessageQueue;
	};

#define LOG_FILE(FilePath,Message) VKCOMMON::Logger::Get().FileLog(FilePath,Message);
#define LOG_FILE(FilePath,Severity,Message) VKCOMMON::Logger::Get().FileLog(FilePath,{Severity,Message});
#define LOG_CONSOLE(FilePath,Message) VKCOMMON::Logger::Get().ConsoleLog(FilePath,Message);
}



