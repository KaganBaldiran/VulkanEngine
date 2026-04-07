#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

#include "VulkanSynchoronization.hpp"
#include "VulkanUtils.hpp" 

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
        std::vector<RENDERER_CORE::BarrierState>& swapChainBarrierStates,
        VkFormat surfaceFormat,
        VkDevice& logicalDevice
    );
}
