#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include "VulkanUtils.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VKCORE
{
    struct VulkanWindowCreateInfo
    {
        unsigned int WindowInitialWidth;
        unsigned int WindowInitialHeight;
        std::string WindowsName;
    };

    /// <summary>
    /// Creates a window with the specified dimensions and name, and sets up a framebuffer resize callback.
    /// </summary>
    /// <param name="Window">A pointer to a GLFWwindow object that will be initialized.</param>
    /// <param name="Width">The width of the window in pixels.</param>
    /// <param name="Height">The height of the window in pixels.</param>
    /// <param name="WindowName">A null-terminated string specifying the window's title.</param>
    /// <param name="FrameBufferResizedCallback">A reference to a boolean that will be set to true if the framebuffer is resized.</param>
    /// <returns>A VkResult value indicating the success or failure of the window creation operation.</returns>
    VulkanResult CreateWindow(GLFWwindow** Window, uint32_t Width, uint32_t Height, const char* WindowName, bool& FrameBufferResizedCallback);

	class Window
	{
	public:
		Window(VulkanWindowCreateInfo &CreateInfo);
        Window() = default;
        void Create(VulkanWindowCreateInfo& CreateInfo);
        void Destroy();
        GLFWwindow* window;
        uint32_t Width;
        uint32_t Height;
        const char* WindowName;
        bool FrameBufferResizedCallback;
        glm::dvec2 MousePosition;
        int IsScrollY;
	private:
	};
}