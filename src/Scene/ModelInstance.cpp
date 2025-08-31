#include "ModelInstance.hpp"
#include "DependencyManager.hpp"

SCENE::ModelInstance::ModelInstance(ModelHandle& Source)
{
	Create(Source);
}

void SCENE::ModelInstance::Create(ModelHandle& Source)
{
	this->resourceType = RESOURCE_TYPE_ENTITY;
	this->Source = &Source;
};