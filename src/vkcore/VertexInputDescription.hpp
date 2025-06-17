#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
    struct VertexInputDescription
    {
        std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;
        VkVertexInputBindingDescription BindingDescription{};

        void AppendAttributeDescription(
            uint32_t Binding,
            uint32_t Location,
            VkFormat Format,
            uint32_t Offset
        )
        {
            VkVertexInputAttributeDescription NewAttributeDescription{};
            NewAttributeDescription.binding = Binding;
            NewAttributeDescription.location = Location;
            NewAttributeDescription.format = Format;
            NewAttributeDescription.offset = Offset;
            AttributeDescriptions.push_back(NewAttributeDescription);
        }

        void SetBindingDescription(
            uint32_t Binding,
            uint32_t Stride,
            VkVertexInputRate InputRate
        )
        {
            BindingDescription.binding = Binding;
            BindingDescription.stride = Stride;
            BindingDescription.inputRate = InputRate;
        }
    };
}