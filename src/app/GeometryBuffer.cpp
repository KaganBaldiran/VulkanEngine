#include "GeometryBuffer.hpp"
#include "../vkcore/VulkanImageView.hpp"

void VKAPP::GeometryBuffer::Create(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, const uint32_t& Width, const uint32_t& Height)
{
    VKCORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        Width,
        Height,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        PositionAttachment.Image,
        PositionAttachment.ImageMemory
    );
    PositionAttachment.ImageView = VKCORE::CreateImageView(PositionAttachment.Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, PositionAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VKCORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        Width,
        Height,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        NormalAttachment.Image,
        NormalAttachment.ImageMemory
    );
    NormalAttachment.ImageView = VKCORE::CreateImageView(NormalAttachment.Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, NormalAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

void VKAPP::GeometryBuffer::Destroy(VkDevice& LogicalDevice)
{
    PositionAttachment.Destroy(LogicalDevice);
    NormalAttachment.Destroy(LogicalDevice);
}
