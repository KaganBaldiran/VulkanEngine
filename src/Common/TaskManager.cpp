#include "TaskManager.hpp"

void COMMON::TaskManager::AddTask(uint32_t DependencyFlagsBit, std::function<void()>& Task)
{
	Tasks[DependencyFlagsBit] = Task;
}
