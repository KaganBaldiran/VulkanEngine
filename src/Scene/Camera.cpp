#include "Camera.hpp"
#include "../Renderer/Core/VulkanWindow.hpp"

#include "DependencyManager.hpp"

SCENE::Camera3D::Camera3D(RENDERER_CORE::Window& window, ResourceDependencyManager& DependencyManager)
{
    Create(window, DependencyManager);
}

void SCENE::Camera3D::Create(RENDERER_CORE::Window& window, ResourceDependencyManager& DependencyManager)
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

    this->dependencyManager = &DependencyManager;
    resourceType = RESOURCE_TYPE_CAMERA;
}

void SCENE::Camera3D::Update(RENDERER_CORE::Window& window,float Sensitivity,float DeltaTime)
{
    if (FirstTurn)
    {
        LastX = window.MousePosition.x;
        LastY = window.MousePosition.y;
        FirstTurn = false;
    }

    if (glfwGetKey(window.window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) AllowPressExit = true;

    CameraRight = glm::normalize(glm::cross(CameraDirection, CameraDirection.y < 0.9999 ? Up : glm::vec3(0.0f, 0.0f, 1.0f)));
    CameraUp = glm::normalize(glm::cross(CameraDirection, CameraRight));

    if (glfwGetKey(window.window, GLFW_KEY_UP) == GLFW_PRESS && AllowMove.x)
    {
        CameraPosition += CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_DOWN) == GLFW_PRESS && AllowMove.w)
    {
        CameraPosition -= CameraDirection * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_LEFT) == GLFW_PRESS && AllowMove.y)
    {
        CameraPosition -= CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_RIGHT) == GLFW_PRESS && AllowMove.z)
    {
        CameraPosition += CameraRight * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        CameraPosition += Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
    {
        CameraPosition -= Up * Sensitivity * DeltaTime;
    }
    if (glfwGetKey(window.window, GLFW_KEY_ESCAPE) == GLFW_PRESS && AllowPressExit)
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

void SCENE::Camera3D::UpdateMatrix(glm::vec2 Extent,float Near,float Far)
{
    ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraDirection, Up);
    ProjectionMatrix = glm::perspective(glm::radians((float)FOV), (float)Extent.x / (float)Extent.y, Near, Far);
    ProjectionMatrix[1][1] *= -1;
}
