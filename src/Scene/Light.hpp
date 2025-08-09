#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SceneResource.hpp"

namespace SCENE
{
	//Forward Declaration
	class ResourceDependencyManager;

	enum LightType
	{
		DIRECTIONAL_LIGHT = 0,
		POINT_LIGHT = 1
	};

	struct alignas(16) LightData
	{
		glm::vec4 Color = glm::vec4(1.0f);
		glm::vec4 PositionOrDirection = glm::vec4(1.0f);;
		float Intensity = 1.0f;
		LightType Type = DIRECTIONAL_LIGHT;
	};

	class Light : public SceneResource
	{
	public:
		Light(ResourceDependencyManager& DependencyManager);
		Light() = default;
		void Create(ResourceDependencyManager& DependencyManager);

		LightData Data;
		bool Updated = false;

		void SetColor(const glm::vec4& Color);
		void SetPosition(const glm::vec4& Position);
		void SetDirection(const glm::vec4& Direction);
		void SetIntensity(const float &Intensity);
		void SetType(const LightType& Type);
	};
}
