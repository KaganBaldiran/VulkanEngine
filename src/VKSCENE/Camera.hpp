#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace VKCORE
{
	//Forward Declarations
	class Window;
}

namespace VKSCENE
{
	class Camera3D
	{
	public:
		Camera3D(VKCORE::Window &window);
		Camera3D() = default;
		void Create(VKCORE::Window& window);
		void Update(VKCORE::Window &window);
		void UpdateMatrix(glm::vec2 Extent);
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
