#include "SceneResource.hpp"
#include "DependencyManager.hpp"

SCENE::SceneResource::SceneResource() : ResourceID(GenerateResourceID())
{
	Name = "Resource" + std::to_string(ResourceID);
}

void SCENE::SceneResource::SetDirty()
{
	assert(dependencyManager != nullptr);
	dependencyManager->MarkResourceDirty(*this);
}

uint64_t SCENE::SceneResource::GenerateResourceID()
{
	static uint64_t Iterator = 1;
	return Iterator++;
}