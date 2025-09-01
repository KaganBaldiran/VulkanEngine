#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SceneResource.hpp"
#include <functional>

namespace RENDERER_CORE
{
	//Forward Declarations
	class Window;
}

namespace SCENE
{
	//Forward Declarations
	class ResourceDependencyManager;

	enum CameraMode
	{
		CAMERA_MODE_UNSPECIFIED = 0,
		CAMERA_MODE_FREE_CAMERA = 1,
		CAMERA_MODE_SCRIPTED_CAMERA = 2
	};

	struct CameraKeyBindingsInfo
    {
		uint16_t ForwardKey = GLFW_KEY_W;
		uint16_t BackKey = GLFW_KEY_S;
		uint16_t LeftKey = GLFW_KEY_A;
		uint16_t RightKey = GLFW_KEY_D;
		uint16_t UpKey = GLFW_KEY_SPACE;
		uint16_t DownKey = GLFW_KEY_LEFT_ALT;
		uint16_t ToggleCameraInputKey = GLFW_KEY_ESCAPE;
	};

	class CameraModeInfo
	{
	public:
		CameraModeInfo() = default;
		virtual ~CameraModeInfo() = default;
	};

	class CameraFreeModeInfo : public CameraModeInfo
	{
	public:
		CameraFreeModeInfo() = default;
		CameraKeyBindingsInfo KeyBindings;
	};

	using CameraScript = std::function<void(glm::vec3 &CameraPosition, glm::vec3 &CameraDirection,double DeltaTime,float Sensitivity,RENDERER_CORE::Window &Window)>;
	class CameraScriptedModeInfo : public CameraModeInfo
	{
	public:
		CameraScriptedModeInfo() = default;
		CameraScript Script;
	};

	struct CameraSettingsInfo
	{
		CameraMode Mode = CAMERA_MODE_UNSPECIFIED;
		CameraModeInfo* CameraModeInfo = nullptr;
	};

	class Camera3D : public Resource
	{
	public:
		Camera3D(RENDERER_CORE::Window &window,CameraSettingsInfo Settings);
		Camera3D() = default;
		void Create(RENDERER_CORE::Window& window,CameraSettingsInfo Settings);
		void SetCameraSettings(CameraSettingsInfo Settings);
		void Update(RENDERER_CORE::Window &window,float Sensitivity,float DeltaTime, glm::vec2 Extent, float Near, float Far);
		glm::vec3 CameraPosition;
		glm::vec3 CameraDirection;
		glm::vec3 Up;

		double Yaw;
		double Pitch;
		double FOV;
		float NearPlane, FarPlane;

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

		void UpdateFreeCameraMode(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime);

		CameraMode Mode;
		CameraKeyBindingsInfo KeyBindings;
		CameraScript Script;
	};
}
