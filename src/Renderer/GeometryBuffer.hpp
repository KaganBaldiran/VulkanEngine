#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "Core/VulkanUtils.hpp"
#include "Core/VulkanImage.hpp"
#include <vector>
#include <map>

namespace RENDERER
{
    struct GeometryBuffer
    {
        RENDERER_CORE::ImageData PositionAttachment;
        RENDERER_CORE::ImageData AlbedoAttachment;
        RENDERER_CORE::ImageData RoughnessMetallicAttachment;
        RENDERER_CORE::ImageData NormalAttachment;

        void Create(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            const uint32_t& Width,
            const uint32_t& Height
        );

        void Destroy(VkDevice& LogicalDevice);
    };
}
