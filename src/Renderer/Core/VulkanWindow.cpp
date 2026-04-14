#include "VulkanWindow.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto Window = reinterpret_cast<RENDERER_CORE::Window*>(glfwGetWindowUserPointer(window));
    Window->FrameBufferResizedCallback = true;
    Window->Width = width;
    Window->Height = height;
}

static void Mouse_Callback(GLFWwindow* window, double xpos, double ypos)
{
    auto Window = reinterpret_cast<RENDERER_CORE::Window*>(glfwGetWindowUserPointer(window));
    Window->MousePosition = {xpos,ypos};
}

static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto Window = reinterpret_cast<RENDERER_CORE::Window*>(glfwGetWindowUserPointer(window));
    if ((float)yoffset > 0.0) Window->IsScrollY = 1;
    else if ((float)yoffset < 0.0) Window->IsScrollY = -1;
    else Window->IsScrollY = 0;
}

RENDERER_CORE::VulkanResult RENDERER_CORE::CreateWindow(GLFWwindow** Window, uint32_t Width, uint32_t Height, const char* WindowName, bool& FrameBufferResizedCallback)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    *Window = glfwCreateWindow(Width, Height, WindowName,nullptr, nullptr);
    if (Window == NULL)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, std::string("Failed creating window [" + std::string(WindowName) + "]."));
        return { VK_INCOMPLETE,"Unable to create window!" };
    }

    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Created window [" + std::string(WindowName) + "]."));
    return VULKAN_SUCCESS;
}

RENDERER_CORE::Window::Window(VulkanWindowCreateInfo& CreateInfo)
{
    Create(CreateInfo);
}

void RENDERER_CORE::Window::Create(VulkanWindowCreateInfo& CreateInfo)
{
    VULKAN_ASSERT_RESULT(CreateWindow(&Handle, CreateInfo.WindowInitialWidth, CreateInfo.WindowInitialHeight, CreateInfo.WindowsName.c_str(), FrameBufferResizedCallback));
    glfwSetWindowUserPointer(Handle, this);
    glfwSetFramebufferSizeCallback(Handle, FramebufferResizeCallback);
    glfwSetCursorPosCallback(Handle, Mouse_Callback);
    glfwSetScrollCallback(Handle, ScrollCallback);

    glfwGetCursorPos(Handle, &MousePosition.x, &MousePosition.y);
    this->WindowName = CreateInfo.WindowsName.c_str();
    this->Width = CreateInfo.WindowInitialWidth;
    this->Height = CreateInfo.WindowInitialHeight;
    this->IsScrollY = false;
    this->FrameBufferResizedCallback = false;
}

void RENDERER_CORE::Window::Destroy()
{
    if (!Handle) return;
    glfwDestroyWindow(Handle);
    Handle = nullptr;
}

GLFWmonitor* RENDERER_CORE::Window::GetMonitor()
{
    return glfwGetPrimaryMonitor();  
}

const GLFWvidmode* RENDERER_CORE::Window::GetMonitorVideoMode()
{
    return glfwGetVideoMode(glfwGetPrimaryMonitor());
}

void RENDERER_CORE::Window::SetFullScreen(bool State)
{
    if (State)
    {
        const GLFWvidmode* Mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowSize(Handle, Mode->width, Mode->height);
        FrameBufferResizedCallback = true;
        Width = Mode->width;
        Height = Mode->height;

        glfwSetWindowMonitor(Handle, glfwGetPrimaryMonitor(), 0, 0, Mode->width, Mode->height, Mode->refreshRate);
    }
}
