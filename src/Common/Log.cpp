#include "Log.hpp"
#include <chrono>
#include <iostream>

VKCOMMON::Logger::Logger()
{
	MessageQueue.reserve(500);
}

VKCOMMON::Logger::~Logger()
{
	std::unique_lock<std::mutex> Lock(Mutex);
	FileFlush();
	for (auto& [FilePath, File] : Files)
	{
		if(File.is_open()) File.close();
	}
}

VKCOMMON::Logger& VKCOMMON::Logger::Get()
{
	static Logger GlobalLogger;
	return GlobalLogger;
}

void VKCOMMON::Logger::FileLog(const char* FilePath,const LogMessage& Message)
{
	std::unique_lock<std::mutex> Lock(Mutex);
	MessageQueue.push_back({ Message,std::string(FilePath) });

	if (Message.Severity >= LOG_SEVERITY_ERROR) FileFlush();
}

void VKCOMMON::Logger::ConsoleLog(const LogMessage& Message)
{
	switch (Message.Severity)
	{
	case LOG_SEVERITY_INFO: std::cout << "Info: "; break;
	case LOG_SEVERITY_WARNING: std::cout << "Warning: "; break;
	case LOG_SEVERITY_ERROR: std::cout << "Error: "; break;
	case LOG_SEVERITY_VERBOSE: std::cout << "Verbose: "; break;
	default: std::cout << "Unknown: "; break;
	}
	std::cout << Message.Message << std::endl;
}

void VKCOMMON::Logger::FileFlush()
{
	for(auto& Message : MessageQueue)
	{
		auto& File = Files[std::string(Message.second)];
		if (!File.is_open()) File.open(Message.second);

		std::string FinalMessage;
		switch (Message.first.Severity)
		{
		case LOG_SEVERITY_INFO:    FinalMessage += "Info: "; break;
		case LOG_SEVERITY_WARNING: FinalMessage += "Warning: "; break;
		case LOG_SEVERITY_ERROR:   FinalMessage += "Error: "; break;
		case LOG_SEVERITY_VERBOSE: FinalMessage += "Verbose: "; break;
		default: FinalMessage += "Unknown: "; break;
		}
		FinalMessage += Message.first.Message + "\n";
		File << FinalMessage;
	}
	MessageQueue.clear();
}
