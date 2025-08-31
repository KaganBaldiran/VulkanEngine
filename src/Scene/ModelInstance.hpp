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
	class ModelInstance : public Resource
	{
	public:
		ModelInstance(ModelHandle& Source);
		ModelInstance() = default;
		void Create(ModelHandle& Source);
		ModelHandle* Source = nullptr;
		Transformation Transformations;
		std::vector<Material> Materials;
	private:
	};
}
