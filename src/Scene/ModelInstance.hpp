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
	class ResourceDependencyManager;

	class ModelInstance : public SceneResource
	{
	public:
		ModelInstance(Model3D& Source);
		ModelInstance() = default;
		void Create(Model3D& Source);
		Model3D* Source = nullptr;
		Transformation Transformations;
	private:
	};
}
