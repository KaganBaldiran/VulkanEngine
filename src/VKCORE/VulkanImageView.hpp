#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
    //Forward Declarations 
    struct VulkanDeviceContext;
    struct VulkanInstanceContext;
    struct VulkanPresentDeviceCreateInfo;
    struct VulkanCreateInfo;
    struct VulkanPresentContext;

    VkImageView CreateImageView(VkImage& Image, VkFormat Format, VkImageAspectFlags AspectMask, VkDevice& LogicalDevice);
    VulkanResult CreateSwapChainImageViews(
        std::vector<VkImage>& swapChainImages,
        std::vector<VkImageView>& swapChainImageViews,
        VkFormat surfaceFormat,
        VkDevice& logicalDevice
    );
}
