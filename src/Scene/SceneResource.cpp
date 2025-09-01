#include "SceneResource.hpp"

SCENE::Resource::Resource() : ResourceID(GenerateResourceID())
{
}

uint64_t SCENE::GenerateResourceID()
{
	static uint64_t Iterator = 1;
	return Iterator++;
}