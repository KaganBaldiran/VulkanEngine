#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <string>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "VulkanUtils.hpp" 

namespace VKCORE
{
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities,GLFWwindow* Window);
    VulkanResult CreateSwapChain(
        VkPhysicalDevice physicalDevice,
        VkDevice logicalDevice,
        VkSurfaceKHR surface,
        GLFWwindow* window,
        VkSwapchainKHR& swapChain,
        std::vector<VkImage>& swapChainImages,
        VkSurfaceFormatKHR& surfaceFormat,
        VkPresentModeKHR& presentMode,
        VkExtent2D& extent
    );

	class SwapChain
	{
	public:
        SwapChain(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            VkSurfaceKHR& Surface,
            GLFWwindow* Window
        );
        void Destroy(VkDevice &LogicalDevice);
        void Create(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            VkSurfaceKHR& Surface,
            GLFWwindow* Window
        );

        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> SwapChainImages;
        std::vector<VkImageView> SwapChainImagesViews;
        VkSurfaceFormatKHR SurfaceFormat;
        VkPresentModeKHR PresentMode;
        VkExtent2D Extent;
	private:
	};
}