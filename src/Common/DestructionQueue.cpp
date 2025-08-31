#include "DestructionQueue.hpp"
#include <algorithm>

void COMMON::DestructionQueue::Destroy()
{
	std::sort(DestructibleList.begin(), DestructibleList.end(), [](Destructible* Destructible0, Destructible* Destructible1) {
		return Destructible0->DestructionPriority < Destructible1->DestructionPriority;
		});

	for (int i = DestructibleList.size() - 1; i >= 0; i--)
	{
		DestructibleList[i]->Destroy();
	}
	DestructibleList.clear();
}

COMMON::DestructionQueue* COMMON::DestructionQueue::Get()
{
	static DestructionQueue GlobalDestructionQueue;
	return &GlobalDestructionQueue;
}

void COMMON::DestructionQueue::Register(Destructible* DestructibleToRegister)
{
	DestructibleList.push_back(DestructibleToRegister);
}
