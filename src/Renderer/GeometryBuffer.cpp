#include "GeometryBuffer.hpp"
#include "Core/VulkanImageView.hpp"

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
    PositionAttachment.ImageView = VKCORE::CreateImageView(PositionAttachment.Image, VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, PositionAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VKCORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        Width,
        Height,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        NormalAttachment.Image,
        NormalAttachment.ImageMemory
    );
    NormalAttachment.ImageView = VKCORE::CreateImageView(NormalAttachment.Image, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, NormalAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VKCORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        Width,
        Height,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        AlbedoAttachment.Image,
        AlbedoAttachment.ImageMemory
    );
    AlbedoAttachment.ImageView = VKCORE::CreateImageView(AlbedoAttachment.Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, AlbedoAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VKCORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        Width,
        Height,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        RoughnessMetallicAttachment.Image,
        RoughnessMetallicAttachment.ImageMemory
    );
    RoughnessMetallicAttachment.ImageView = VKCORE::CreateImageView(RoughnessMetallicAttachment.Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, RoughnessMetallicAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

void VKAPP::GeometryBuffer::Destroy(VkDevice& LogicalDevice)
{
    PositionAttachment.Destroy(LogicalDevice);
    NormalAttachment.Destroy(LogicalDevice);
    RoughnessMetallicAttachment.Destroy(LogicalDevice);
    AlbedoAttachment.Destroy(LogicalDevice);
}
