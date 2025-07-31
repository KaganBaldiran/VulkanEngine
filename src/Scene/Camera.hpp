#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SceneResource.hpp"

namespace VKCORE
{
	//Forward Declarations
	class Window;
}

namespace VKSCENE
{
	//Forward Declarations
	class ResourceDependencyManager;

	class Camera3D : public SceneResource
	{
	public:
		Camera3D(VKCORE::Window &window , ResourceDependencyManager& DependencyManager);
		Camera3D() = default;
		void Create(VKCORE::Window& window,ResourceDependencyManager& DependencyManager);
		void Update(VKCORE::Window &window,float Sensitivity,float DeltaTime);
		void UpdateMatrix(glm::vec2 Extent, float Near, float Far);
		glm::vec3 CameraPosition;
		glm::vec3 CameraDirection;
		glm::vec3 Up;

		double Yaw;
		double Pitch;
		double FOV;

		double LastX;
		double LastY;

		glm::mat4 ViewMatrix;
		glm::mat4 ProjectionMatrix;

		glm::vec4 AllowMove;
		glm::vec3 CameraRight;
		glm::vec3 CameraUp;
	private:
		bool CursorDisabled = true;
		bool AllowPressExit = true;
		bool FirstTurn = true;
	};
}
