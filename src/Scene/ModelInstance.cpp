#include "ModelInstance.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

SCENE::ModelInstance::ModelInstance(ModelHandle& Source)
{
	Create(Source);
}

void SCENE::ModelInstance::Create(ModelHandle& Source)
{
	this->Source = &Source;
	if (Source.Meshes.empty())
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING,
			std::string("Attempting to instance a model with no meshes (Model ID:" + std::to_string(Source.GetHandleID()) +")."));
		return;
	}

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
	if (Index >= Materials.size())
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING,
			std::string("Attempting to fetch a material with an index that exceeds the available material index limit (Requested index:" 
				+ std::to_string(Index) + "available material count: " + std::to_string(Materials.size()) + ")."));
		return nullptr;
	}
	return &Materials[Index];
}