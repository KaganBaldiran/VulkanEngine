#include "ModelInstance.hpp"

SCENE::ModelInstance::ModelInstance(ModelHandle& Source)
{
	Create(Source);
}

void SCENE::ModelInstance::Create(ModelHandle& Source)
{
	this->Source = &Source;
	if (Source.Meshes.empty()) return;

	Materials.resize(Source.Meshes.size());
	for (size_t i = 0; i < Source.Meshes.size(); i++)
	{
		Materials[i] = Source.Meshes[i].MeshMaterial;
	}
}
void SCENE::ModelInstance::ReferenceModel(ModelHandle& Source)
{
	Create(Source);
}

SCENE::Material* SCENE::ModelInstance::GetMaterial(size_t Index)
{
	if (Index >= Materials.size()) return nullptr;
	return &Materials[Index];
}