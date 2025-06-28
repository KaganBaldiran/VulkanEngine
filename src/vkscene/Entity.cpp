#include "Entity.hpp"
#include "DependencyManager.hpp"

VKSCENE::Entity::Entity(ResourceDependencyManager& DependencyManager)
{
	Create(DependencyManager);
}

void VKSCENE::Entity::Create(ResourceDependencyManager& DependencyManager)
{
	this->dependencyManager = &DependencyManager;
	this->resourceType = RESOURCE_TYPE_ENTITY;
};