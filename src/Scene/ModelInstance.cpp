#include "ModelInstance.hpp"

SCENE::ModelInstance::ModelInstance(ModelHandle& Source)
{
	Create(Source);
}

void SCENE::ModelInstance::Create(ModelHandle& Source)
{
	this->Source = &Source;
};