#pragma once
#include <functional>
#include <unordered_map>

namespace COMMON
{
	class TaskManager
	{
	public:
		void AddTask(uint32_t DependencyFlagsBit,std::function<void()>& Task);
		void Fire(uint32_t DependencyFlagsBit);
	private:
		std::unordered_map<uint32_t,std::function<void()>> Tasks;
	};
}