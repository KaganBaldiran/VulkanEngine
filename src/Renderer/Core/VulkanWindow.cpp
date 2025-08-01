#include "VulkanWindow.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto Window = reinterpret_cast<VKCORE::Window*>(glfwGetWindowUserPointer(window));
    Window->FrameBufferResizedCallback = true;
    Window->Width = width;
    Window->Height = height;
}

static void Mouse_Callback(GLFWwindow* window, double xpos, double ypos)
{
    auto Window = reinterpret_cast<VKCORE::Window*>(glfwGetWindowUserPointer(window));
    Window->MousePosition = {xpos,ypos};
}

static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto Window = reinterpret_cast<VKCORE::Window*>(glfwGetWindowUserPointer(window));
    if ((float)yoffset > 0.0) Window->IsScrollY = 1;
    else if ((float)yoffset < 0.0) Window->IsScrollY = -1;
    else Window->IsScrollY = 0;
}

VKCORE::VulkanResult VKCORE::CreateWindow(GLFWwindow** Window, uint32_t Width, uint32_t Height, const char* WindowName, bool& FrameBufferResizedCallback)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    *Window = glfwCreateWindow(Width, Height, WindowName, nullptr, nullptr);
    if (Window == NULL)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_VERBOSE, std::string("Failed creating window [" + std::string(WindowName) + "]."));
        return { VK_INCOMPLETE,"Unable to create window!" };
    }
    LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_INFO, std::string("Created window [" + std::string(WindowName) + "]."));
    return VULKAN_SUCCESS;
}

VKCORE::Window::Window(VulkanWindowCreateInfo& CreateInfo)
{
    Create(CreateInfo);
}

void VKCORE::Window::Create(VulkanWindowCreateInfo& CreateInfo)
{
    VULKAN_ASSERT_RESULT(CreateWindow(&window, CreateInfo.WindowInitialWidth, CreateInfo.WindowInitialHeight, CreateInfo.WindowsName.c_str(), FrameBufferResizedCallback));
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
    glfwSetCursorPosCallback(window, Mouse_Callback);
    glfwSetScrollCallback(window, ScrollCallback);

    glfwGetCursorPos(window, &MousePosition.x, &MousePosition.y);
    this->WindowName = CreateInfo.WindowsName.c_str();
    this->Width = CreateInfo.WindowInitialWidth;
    this->Height = CreateInfo.WindowInitialHeight;
    this->IsScrollY = false;
    this->FrameBufferResizedCallback = false;
}

void VKCORE::Window::Destroy()
{
    if (!window) return;
    glfwDestroyWindow(window);
    window = nullptr;
}
