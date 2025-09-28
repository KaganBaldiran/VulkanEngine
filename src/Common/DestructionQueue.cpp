#include "DestructionQueue.hpp"
#include <algorithm>

#include "../Renderer/RendererContext.hpp"

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

void COMMON::DestroyResources(RENDERER::RendererContext& RendererContext)
{
	RendererContext.WaitDeviceIdle();
	COMMON::DestructionQueue::Get()->Destroy();
}
