#include "ModelInstance.hpp"
#include "DependencyManager.hpp"

SCENE::ModelInstance::ModelInstance(Model3D& Source)
{
	Create(Source);
}

void SCENE::ModelInstance::Create(Model3D& Source)
{
	this->resourceType = RESOURCE_TYPE_ENTITY;
	this->Source = &Source;
};