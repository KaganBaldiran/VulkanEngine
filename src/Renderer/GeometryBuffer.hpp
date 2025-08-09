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
        RENDERER_CORE::TextureData PositionAttachment;
        RENDERER_CORE::TextureData AlbedoAttachment;
        RENDERER_CORE::TextureData RoughnessMetallicAttachment;
        RENDERER_CORE::TextureData NormalAttachment;

        void Create(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            const uint32_t& Width,
            const uint32_t& Height
        );

        void Destroy(VkDevice& LogicalDevice);
    };
}
