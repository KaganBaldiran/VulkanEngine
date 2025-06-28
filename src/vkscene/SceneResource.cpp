#include "SceneResource.hpp"
#include "DependencyManager.hpp"

VKSCENE::SceneResource::SceneResource() : ResourceID(GenerateResourceID())
{
	Name = "Resource" + std::to_string(ResourceID);
}

void VKSCENE::SceneResource::SetDirty()
{
	assert(dependencyManager != nullptr);
	dependencyManager->MarkResourceDirty(*this);
}

uint64_t VKSCENE::SceneResource::GenerateResourceID()
{
	static uint64_t Iterator = 0;
	return Iterator++;
}