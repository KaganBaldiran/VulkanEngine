#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace RENDERER_CORE
{
    //Forward Declarations 
    struct VulkanDeviceContext;
    struct VulkanInstanceContext;
    struct VulkanPresentDeviceCreateInfo;
    struct VulkanCreateInfo;
    struct VulkanPresentContext;

    VkImageView CreateImageView(
        VkImage& Image, 
        VkFormat Format, 
        VkImageViewType ViewType,
        VkImageAspectFlags AspectMask, 
        VkDevice& LogicalDevice,
        uint32_t LayerCount = 1,
        uint32_t BaseArrayLayer = 0
    );

    VulkanResult CreateSwapChainImageViews(
        std::vector<VkImage>& swapChainImages,
        std::vector<VkImageView>& swapChainImageViews,
        std::vector<VkImageLayout>& swapChainLayouts,
        VkFormat surfaceFormat,
        VkDevice& logicalDevice
    );
}
