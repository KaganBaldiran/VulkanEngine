#include "Camera.hpp"
#include "../Renderer/Core/VulkanWindow.hpp"

SCENE::Camera3D::Camera3D(RENDERER_CORE::Window& window,CameraSettingsInfo Settings)
{
    Create(window,Settings);
}

void SCENE::Camera3D::Create(RENDERER_CORE::Window& window,CameraSettingsInfo Settings)
{
    CameraPosition = { 0.0f,0.0f,0.0f };
    CameraDirection = { 0.0f,0.0f,-1.0f };
    Up = { 0.0f,1.0f,0.0f };

    Yaw = -90.0f;
    Pitch = 0.0f;
    FOV = 45.0;

    LastX = window.Width / 2.0;
    LastY = window.Height / 2.0;
    CursorDisabled = true;
    AllowPressExit = true;
    FirstTurn = true;
    AllowMove = glm::vec4(1.0f);

    resourceType = RESOURCE_TYPE_CAMERA;
    SetCameraSettings(Settings);
}

void SCENE::Camera3D::SetCameraSettings(CameraSettingsInfo Settings)
{
    this->Mode = Settings.Mode;
    switch (Settings.Mode)
    {
    case CAMERA_MODE_UNSPECIFIED:
    {
        throw std::runtime_error("Unset camera settings! Unable to initialize camera!");
        exit(-1);
        break;
    }
    case CAMERA_MODE_FREE_CAMERA:
    {
        if (auto ModeInfo = dynamic_cast<CameraFreeModeInfo*>(Settings.CameraModeInfo))
        {
            this->KeyBindings = ModeInfo->KeyBindings;
        }
        break;
    }
    case CAMERA_MODE_SCRIPTED_CAMERA:
    {
        if (auto ModeInfo = dynamic_cast<CameraScriptedModeInfo*>(Settings.CameraModeInfo))
        {
            this->Script = ModeInfo->Script;
        }
        break;
    }
    default:
        break;
    }
}

void SCENE::Camera3D::Update(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime, glm::vec2 Extent, float Near, float Far)
{
    this->FarPlane = Far;
    this->NearPlane = Near;

    switch (this->Mode)
    {
    case CAMERA_MODE_UNSPECIFIED:
    {
        throw std::runtime_error("Unset camera settings! Unable to update camera!");
        exit(-1);
        break;
    }
    case CAMERA_MODE_FREE_CAMERA:
    {
        UpdateFreeCameraMode(window, Sensitivity, DeltaTime);
        break;
    }
    case CAMERA_MODE_SCRIPTED_CAMERA:
    {
        Script(CameraPosition, CameraDirection, DeltaTime, Sensitivity, window);
        break;
    }
    default:
        break;
    }

    ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraDirection, Up);
    ProjectionMatrix = glm::perspective(glm::radians((float)FOV), (float)Extent.x / (float)Extent.y, Near, Far);
    ProjectionMatrix[1][1] *= -1;
}

void SCENE::Camera3D::UpdateFreeCameraMode(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime)
{
    if (FirstTurn)
    {
        LastX = window.MousePosition.x;
        LastY = window.MousePosition.y;
        FirstTurn = false;
    }

    if (glfwGetKey(window.window, KeyBindings.ToggleCameraInputKey) == GLFW_RELEASE) AllowPressExit = true;

    CameraRight = glm::normalize(glm::cross(CameraDirection, CameraDirection.y < 0.9999 ? Up : glm::vec3(0.0f, 0.0f, 1.0f)));
    CameraUp = glm::normalize(glm::cross(CameraDirection, CameraRight));

    if (glfwGetKey(window.window, KeyBindings.ForwardKey) == GLFW_PRESS && AllowMove.x)
    {
        CameraPosition += CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.BackKey) == GLFW_PRESS && AllowMove.w)
    {
        CameraPosition -= CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.LeftKey) == GLFW_PRESS && AllowMove.y)
    {
        CameraPosition -= CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.RightKey) == GLFW_PRESS && AllowMove.z)
    {
        CameraPosition += CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.UpKey) == GLFW_PRESS)
    {
        CameraPosition += Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.DownKey) == GLFW_PRESS)
    {
        CameraPosition -= Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, KeyBindings.ToggleCameraInputKey) == GLFW_PRESS && AllowPressExit)
    {
        CursorDisabled = !CursorDisabled;
        AllowPressExit = false;
    }

    if (CursorDisabled)
    {
        glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        double Xoffset = window.MousePosition.x - LastX;
        double Yoffset = window.MousePosition.y - LastY;
        LastX = window.MousePosition.x;
        LastY = window.MousePosition.y;

        Xoffset *= Sensitivity * DeltaTime;
        Yoffset *= Sensitivity * DeltaTime;

        Yaw += Xoffset;
        Pitch -= Yoffset;

        Pitch = glm::clamp(Pitch, -89.0, 89.0);

        CameraDirection.x = glm::cos(glm::radians(Yaw)) * glm::cos(glm::radians(Pitch));
        CameraDirection.z = glm::sin(glm::radians(Yaw)) * glm::cos(glm::radians(Pitch));
        CameraDirection.y = glm::sin(glm::radians(Pitch));
        CameraDirection = glm::normalize(CameraDirection);

        FOV -= (double)window.IsScrollY * DeltaTime * Sensitivity;
        window.IsScrollY = 0;
        FOV = glm::clamp(FOV, 0.1, 180.0);
    }
    else
    {
        glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

