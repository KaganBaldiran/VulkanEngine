#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <queue>

#include "SceneResource.hpp"
#include "Mesh.hpp"

namespace SCENE
{
	class SceneMeshManager;

	class ModelInstance : public Resource
	{
	public:
		ModelInstance(ModelHandle& Source);
		ModelInstance() = default;
		void Create(ModelHandle& Source);
		void ReferenceModel(ModelHandle& Source);
		Material* GetMaterial(size_t Index);
		Transformation* GetTransformations() { return &Transformations; };
		ModelHandle* Source = nullptr;
		Transformation Transformations;
		std::vector<Material> Materials;             
	};
}
