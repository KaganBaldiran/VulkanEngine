#include "Camera.hpp"
#include "../Renderer/Core/VulkanWindow.hpp"

SCENE::Camera3D::Camera3D(CameraSettingsInfo Settings)
{
    Create(Settings);
}

void SCENE::Camera3D::Create(CameraSettingsInfo Settings)
{
    CameraPosition = { 0.0f,0.0f,0.0f };
    CameraDirection = { 0.0f,0.0f,-1.0f };
    Up = { 0.0f,1.0f,0.0f };

    Yaw = -90.0f;
    Pitch = 0.0f;
    FOV = 45.0;

    //LastX = window.Width / 2.0;
    //LastY = window.Height / 2.0;
    CursorDisabled = true;
    AllowPressExit = true;
    FirstTurn = true;
    AllowMove = glm::vec4(1.0f);

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

void SCENE::Camera3D::Update(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime, glm::vec2 Extent, float Near, float Far, float FOV)
{
    this->FarPlane = Far;
    this->NearPlane = Near;
    this->FOV = FOV;

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

void SCENE::Camera3D::Update(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime,float Zoom,glm::vec2 Extent, float Near, float Far)
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

    float Ratio = (float)Extent.x / (float)Extent.y;
    ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraDirection, Up);
    ProjectionMatrix = glm::ortho(-Zoom * Ratio, Zoom * Ratio,-Zoom,Zoom,Near,Far);
    ProjectionMatrix[1][1] *= -1;
}

void SCENE::Camera3D::ExtractFrustumPlanes(std::array<glm::vec4, 6>& Planes)
{
    glm::mat4 ViewProjection = ProjectionMatrix * ViewMatrix;
    Planes[0] = glm::vec4(
        ViewProjection[0][3] + ViewProjection[0][0],
        ViewProjection[1][3] + ViewProjection[1][0],
        ViewProjection[2][3] + ViewProjection[2][0],
        ViewProjection[3][3] + ViewProjection[3][0]
    );
    Planes[1] = glm::vec4(
        ViewProjection[0][3] - ViewProjection[0][0],
        ViewProjection[1][3] - ViewProjection[1][0],
        ViewProjection[2][3] - ViewProjection[2][0],
        ViewProjection[3][3] - ViewProjection[3][0]
    );
    Planes[2] = glm::vec4(
        ViewProjection[0][3] + ViewProjection[0][1],
        ViewProjection[1][3] + ViewProjection[1][1],
        ViewProjection[2][3] + ViewProjection[2][1],
        ViewProjection[3][3] + ViewProjection[3][1]
    );
    Planes[3] = glm::vec4(
        ViewProjection[0][3] - ViewProjection[0][1],
        ViewProjection[1][3] - ViewProjection[1][1],
        ViewProjection[2][3] - ViewProjection[2][1],
        ViewProjection[3][3] - ViewProjection[3][1]
    );
    Planes[4] = glm::vec4(
        ViewProjection[0][2],
        ViewProjection[1][2],
        ViewProjection[2][2],
        ViewProjection[3][2]
    );
    Planes[5] = glm::vec4(
        ViewProjection[0][3] - ViewProjection[0][2],
        ViewProjection[1][3] - ViewProjection[1][2],
        ViewProjection[2][3] - ViewProjection[2][2],
        ViewProjection[3][3] - ViewProjection[3][2]
    );
    for (size_t i = 0; i < Planes.size(); i++)
    {
        float Length = glm::length(glm::vec3(Planes[i]));
        Planes[i] /= Length;
    }
}

void SCENE::Camera3D::UpdateFreeCameraMode(RENDERER_CORE::Window& window, float Sensitivity, float DeltaTime)
{
    if (FirstTurn)
    {
        LastX = window.MousePosition.x;
        LastY = window.MousePosition.y;
        FirstTurn = false;
    }

    if (glfwGetKey(window.Handle, KeyBindings.ToggleCameraInputKey) == GLFW_RELEASE) AllowPressExit = true;

    CameraRight = glm::normalize(glm::cross(CameraDirection, CameraDirection.y < 0.9999 ? Up : glm::vec3(0.0f, 0.0f, 1.0f)));
    CameraUp = glm::normalize(glm::cross(CameraDirection, CameraRight));

    if (glfwGetKey(window.Handle, KeyBindings.ForwardKey) == GLFW_PRESS && AllowMove.x)
    {
        CameraPosition += CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.BackKey) == GLFW_PRESS && AllowMove.w)
    {
        CameraPosition -= CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.LeftKey) == GLFW_PRESS && AllowMove.y)
    {
        CameraPosition -= CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.RightKey) == GLFW_PRESS && AllowMove.z)
    {
        CameraPosition += CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.UpKey) == GLFW_PRESS)
    {
        CameraPosition += Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.DownKey) == GLFW_PRESS)
    {
        CameraPosition -= Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.Handle, KeyBindings.ToggleCameraInputKey) == GLFW_PRESS && AllowPressExit)
    {
        CursorDisabled = !CursorDisabled;
        AllowPressExit = false;
    }

    if (CursorDisabled)
    {
        glfwSetInputMode(window.Handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
        glfwSetInputMode(window.Handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

