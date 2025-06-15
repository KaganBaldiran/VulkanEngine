#pragma once
#include "VulkanImageView.hpp"
#include <vector>

VkImageView VKCORE::CreateImageView(VkImage& Image, VkFormat Format, VkImageAspectFlags AspectMask, VkDevice& LogicalDevice)
{
    VkImageViewCreateInfo ImageViewCreateInfo{};
    ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImageViewCreateInfo.format = Format;
    ImageViewCreateInfo.image = Image;
    ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    ImageViewCreateInfo.subresourceRange.levelCount = 1;
    ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewCreateInfo.subresourceRange.layerCount = 1;
    ImageViewCreateInfo.subresourceRange.aspectMask = AspectMask;
    ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    VkImageView ImageView;
    if (vkCreateImageView(LogicalDevice, &ImageViewCreateInfo, nullptr, &ImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an image view!");
    }
    return ImageView;
}

VKCORE::VulkanResult VKCORE::CreateSwapChainImageViews(
    std::vector<VkImage>& swapChainImages,
    std::vector<VkImageView>& swapChainImageViews,
    VkFormat surfaceFormat,
    VkDevice& logicalDevice
)
{
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        swapChainImageViews[i] = VKCORE::CreateImageView(
            swapChainImages[i],
            surfaceFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            logicalDevice
        );
    }
    return VULKAN_SUCCESS;
}