#include "ModelInstance.hpp"
#include "DependencyManager.hpp"

VKSCENE::ModelInstance::ModelInstance(Model3D& Source)
{
	Create(Source);
}

void VKSCENE::ModelInstance::Create(Model3D& Source)
{
	this->resourceType = RESOURCE_TYPE_ENTITY;
	this->Source = &Source;
};