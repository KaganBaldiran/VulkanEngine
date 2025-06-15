#include "Camera.hpp"
#include "../VKCORE/VulkanWindow.hpp"

VKSCENE::Camera3D::Camera3D(VKCORE::Window& window)
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
}

void VKSCENE::Camera3D::Update(VKCORE::Window& window)
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

    const float PositionSensitivity = 0.1f;

    if (glfwGetKey(window.window, GLFW_KEY_UP) == GLFW_PRESS && AllowMove.x)
    {
        CameraPosition += CameraDirection * PositionSensitivity;
    }
    if (glfwGetKey(window.window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        CameraPosition -= CameraDirection * PositionSensitivity;
    }
    if (glfwGetKey(window.window, GLFW_KEY_LEFT) == GLFW_PRESS && AllowMove.y)
    {
        CameraPosition -= CameraRight * PositionSensitivity;
    }
    if (glfwGetKey(window.window, GLFW_KEY_RIGHT) == GLFW_PRESS && AllowMove.z)
    {
        CameraPosition += CameraRight * PositionSensitivity;
    }
    if (glfwGetKey(window.window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        CameraPosition += Up * PositionSensitivity;
    }
    if (glfwGetKey(window.window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
    {
        CameraPosition -= Up * PositionSensitivity;
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

        const float Sensitivity = 0.1f;
        Xoffset *= Sensitivity;
        Yoffset *= Sensitivity;

        Yaw += Xoffset;
        Pitch -= Yoffset;

        Pitch = glm::clamp(Pitch, -89.0, 89.0);

        CameraDirection.x = glm::cos(glm::radians(Yaw)) * glm::cos(glm::radians(Pitch));
        CameraDirection.z = glm::sin(glm::radians(Yaw)) * glm::cos(glm::radians(Pitch));
        CameraDirection.y = glm::sin(glm::radians(Pitch));
        CameraDirection = glm::normalize(CameraDirection);

        FOV -= (double)window.IsScrollY;
        window.IsScrollY = 0;
        FOV = glm::clamp(FOV, 0.1, 180.0);
    }
    else
    {
        glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void VKSCENE::Camera3D::UpdateMatrix(glm::vec2 Extent)
{
    ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraDirection, Up);
    ProjectionMatrix = glm::perspective(glm::radians((float)FOV), (float)Extent.x / (float)Extent.y, 0.01f, 1000.0f);
    ProjectionMatrix[1][1] *= -1;
}
