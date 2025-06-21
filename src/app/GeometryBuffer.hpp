#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../vkcore/VulkanUtils.hpp"
#include "../vkcore/VulkanImage.hpp"
#include <vector>
#include <map>

namespace VKAPP
{
    struct GeometryBuffer
    {
        VKCORE::TextureData PositionAttachment;
        VKCORE::TextureData NormalAttachment;

        void Create(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            const uint32_t& Width,
            const uint32_t& Height
        );

        void Destroy(VkDevice& LogicalDevice);
    };
}
