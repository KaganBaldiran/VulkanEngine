#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <queue>

#include "SceneResource.hpp"
#include "Mesh.hpp"

namespace VKSCENE
{
	class ResourceDependencyManager;

	class Entity : public SceneResource
	{
	public:
		Entity(ResourceDependencyManager& DependencyManager);
		Entity() = default;
		void Create(ResourceDependencyManager& DependencyManager);
		VKSCENE::Model3D Model;


	private:
	};
}
