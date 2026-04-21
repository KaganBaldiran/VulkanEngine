#pragma once
#include "VulkanImage.hpp"
#include "VulkanBuffer.hpp"
//#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"
#include "VulkanImageView.hpp"
#include "VulkanCommandBuffer.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

int RENDERER_CORE::ReadTexture(const char* FileName, RENDERER_CORE::RawImageData& DestinationImageData)
{
    stbi_set_flip_vertically_on_load(true);
    DestinationImageData.Pixels = stbi_load(FileName, &DestinationImageData.Width, &DestinationImageData.Height, &DestinationImageData.ChannelCount, STBI_rgb_alpha);
    if (!DestinationImageData.Pixels)
    {
        std::cout << "Unable to load the image(" + std::string(FileName) + ")" << std::endl;
        return -1;
    }
    return 0;
}

VkFormat RENDERER_CORE::FindSupportedFormat(VkPhysicalDevice &PhysicalDevice,const std::vector<VkFormat>& Candidates, VkImageTiling Tiling, VkFormatFeatureFlags Features)
{
    for (auto Format : Candidates)
    {
        VkFormatProperties Properties;
        vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, &Properties);

        if (Tiling == VK_IMAGE_TILING_LINEAR && (Properties.linearTilingFeatures & Features) == Features)
        {
            return Format;
        }
        else if (Tiling == VK_IMAGE_TILING_OPTIMAL && (Properties.optimalTilingFeatures & Features) == Features)
        {
            return Format;
        }
    }
    throw std::runtime_error("Unable to find a suitable format!");
}

void RENDERER_CORE::CreateImage(
    VkPhysicalDevice &PhysicalDevice, 
    VkDevice& LogicalDevice, 
    const uint32_t& Width, 
    const uint32_t& Height, 
    VkImageTiling Tiling, 
    VkFormat Format, 
    VkImageUsageFlags Usage, 
    VkMemoryPropertyFlags Properties, 
    VkImage& Image,
    VkDeviceMemory& ImageMemory,
    uint32_t LayerCount,
    VkImageCreateFlags Flags,
    VkSampleCountFlagBits SampleCount
)
{
    VkImageCreateInfo ImageCreateInfo{};
    ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = Format;
    ImageCreateInfo.mipLevels = 1;
    ImageCreateInfo.extent.width = static_cast<uint32_t>(Width);
    ImageCreateInfo.extent.height = static_cast<uint32_t>(Height);
    ImageCreateInfo.extent.depth = 1;
    ImageCreateInfo.arrayLayers = LayerCount;
    ImageCreateInfo.tiling = Tiling;
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageCreateInfo.usage = Usage;
    ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.samples = SampleCount;
    ImageCreateInfo.flags = Flags;

    if (vkCreateImage(LogicalDevice, &ImageCreateInfo, nullptr, &Image) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed to create image [(" + std::to_string(Width) +
            "x" + std::to_string(Height) + "),format(" + string_VkFormat(Format) + ")]."));
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements ImageMemoryRequirements;
    vkGetImageMemoryRequirements(LogicalDevice, Image, &ImageMemoryRequirements);

    VkMemoryAllocateInfo ImageMemoryAllocationInfo{};
    ImageMemoryAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ImageMemoryAllocationInfo.allocationSize = ImageMemoryRequirements.size;
    ImageMemoryAllocationInfo.memoryTypeIndex = RENDERER_CORE::FindMemoryType(PhysicalDevice,ImageMemoryRequirements.memoryTypeBits, Properties);

    if (vkAllocateMemory(LogicalDevice, &ImageMemoryAllocationInfo, nullptr, &ImageMemory) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed to allocate memory for image [size(" +
                std::to_string(ImageMemoryRequirements.size) + "),format(" + string_VkFormat(Format) + ")]."));
        throw std::runtime_error("Failed to allocate memory for the image");
    }

    vkBindImageMemory(LogicalDevice, Image, ImageMemory, 0);
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Image created [(" + std::to_string(Width) +
        "x" + std::to_string(Height) + "),format(" + string_VkFormat(Format) + ")]."));
}

void RENDERER_CORE::BlitImage(
    VkCommandBuffer &CommandBuffer,
    VkImage SrcImage,
    VkImage DstImage,
    glm::ivec3 SrcOffset,
    glm::ivec3 SrcSizes,
    glm::ivec3 DstOffset,
    glm::ivec3 DstSizes,
    VkImageAspectFlags SrcAspectMask,
    uint32_t SrcMipLevel,
    uint32_t SrcBaseArrayLayer,
    uint32_t SrcLayerCount,
    VkImageAspectFlags DstAspectMask,
    uint32_t DstMipLevel,
    uint32_t DstBaseArrayLayer,
    uint32_t DstLayerCount,
    VkFilter Filter
)
{
    VkImageBlit Blit{};
    Blit.srcOffsets[0] = { 0,0,0 };
    Blit.srcOffsets[1] = { SrcSizes.x,SrcSizes.y,SrcSizes.z };
    Blit.srcSubresource.aspectMask = SrcAspectMask;
    Blit.srcSubresource.mipLevel = SrcMipLevel;
    Blit.srcSubresource.baseArrayLayer = SrcBaseArrayLayer;
    Blit.srcSubresource.layerCount = SrcLayerCount;
    Blit.dstOffsets[0] = { DstOffset.x,DstOffset.y,DstOffset.z };
    Blit.dstOffsets[1] = { DstSizes.x,DstSizes.y ,DstSizes.z };
    Blit.dstSubresource.aspectMask = DstAspectMask;
    Blit.dstSubresource.mipLevel = DstMipLevel;
    Blit.dstSubresource.baseArrayLayer = DstBaseArrayLayer;
    Blit.dstSubresource.layerCount = DstLayerCount;

    vkCmdBlitImage(
        CommandBuffer,
        SrcImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        DstImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        1, 
        &Blit, 
        Filter
    );
}

void RENDERER_CORE::TransitionImageLayout(VkCommandBuffer& DstCommandBuffer, VkImage& Image, VkImageLayout OldLayout, VkImageLayout NewLayout, VkAccessFlags SrcAccessMask,
    VkAccessFlags DstAccessMask, VkPipelineStageFlags SrcStage, VkPipelineStageFlags DstStage, VkImageAspectFlags AspectMask,uint32_t LayerCount,uint32_t BaseArrayLayer)
{
    VkImageMemoryBarrier ImageBarrier{};
    ImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ImageBarrier.oldLayout = OldLayout;
    ImageBarrier.newLayout = NewLayout;
    ImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageBarrier.image = Image;
    ImageBarrier.subresourceRange.aspectMask = AspectMask;
    ImageBarrier.subresourceRange.baseMipLevel = 0;
    ImageBarrier.subresourceRange.levelCount = 1;
    ImageBarrier.subresourceRange.baseArrayLayer = BaseArrayLayer;
    ImageBarrier.subresourceRange.layerCount = LayerCount;
    ImageBarrier.srcAccessMask = SrcAccessMask;
    ImageBarrier.dstAccessMask = DstAccessMask;

    vkCmdPipelineBarrier(DstCommandBuffer, SrcStage, DstStage
        , 0, 0, nullptr, 0, nullptr, 1, &ImageBarrier);
}

void RENDERER_CORE::CreateTextureSampler(VkPhysicalDevice& PhysicalDevice,VkDevice &LogicalDevice,VkSampler &DestinationSampler,VkFilter Filter,VkSamplerAddressMode AddressMode, VkSamplerMipmapMode MipmapMode)
{
    VkPhysicalDeviceProperties DeviceProperties;
    vkGetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);

    VkSamplerCreateInfo SamplerCreateInfo{};
    SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    SamplerCreateInfo.magFilter = Filter;
    SamplerCreateInfo.minFilter = Filter;
    SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    SamplerCreateInfo.addressModeU = AddressMode;
    SamplerCreateInfo.addressModeV = AddressMode;
    SamplerCreateInfo.addressModeW = AddressMode;
    SamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    SamplerCreateInfo.maxAnisotropy = DeviceProperties.limits.maxSamplerAnisotropy;
    SamplerCreateInfo.anisotropyEnable = VK_TRUE;
    SamplerCreateInfo.compareEnable = VK_FALSE;
    SamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    SamplerCreateInfo.mipmapMode = MipmapMode;
    SamplerCreateInfo.mipLodBias = 0.0f;
    SamplerCreateInfo.minLod = 0.0f;
    SamplerCreateInfo.maxLod = 0.0f;

    if (vkCreateSampler(LogicalDevice, &SamplerCreateInfo, nullptr, &DestinationSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

void RENDERER_CORE::CopyBufferToImage(VkCommandBuffer& DstCommandBuffer, VkBuffer& SrcBuffer, VkImage& DstImage, uint32_t Width, uint32_t Height)
{
    VkBufferImageCopy CopyRegion{};
    CopyRegion.bufferOffset = 0;
    CopyRegion.bufferRowLength = 0;
    CopyRegion.bufferImageHeight = 0;

    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.mipLevel = 0;
    CopyRegion.imageSubresource.baseArrayLayer = 0;
    CopyRegion.imageSubresource.layerCount = 1;

    CopyRegion.imageOffset = { 0,0,0 };
    CopyRegion.imageExtent = { Width,Height,1 };

    vkCmdCopyBufferToImage(DstCommandBuffer, SrcBuffer, DstImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);
}

void RENDERER_CORE::CreateTextureImage(const char* ImageFilePath,VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice,VkCommandPool &CommandPool,VkQueue &GraphicsQueue,ImageData &DestinationTexture)
{
    stbi_set_flip_vertically_on_load(true);
    RawImageData ImageData;
    ImageData.Pixels = stbi_load(ImageFilePath, &ImageData.Width, &ImageData.Height, &ImageData.ChannelCount, STBI_rgb_alpha);
    VkDeviceSize ImageSize = ImageData.Width * ImageData.Height * 4;

    if (!ImageData.Pixels)
    {
        throw std::runtime_error("Unable to load the image(" + std::string(ImageFilePath) + ")");
    }
    CreateTextureImage(ImageData, PhysicalDevice, LogicalDevice, CommandPool, GraphicsQueue, DestinationTexture);
    stbi_image_free(ImageData.Pixels);
}

void RENDERER_CORE::CreateTextureImage(RawImageData& RawImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, ImageData& DestinationTexture)
{
    VkDeviceSize ImageSize = RawImageData.Width * RawImageData.Height * 4;

    RENDERER_CORE::Buffer StagingBuffer;
    RENDERER_CORE::CreateBuffer(PhysicalDevice, LogicalDevice, ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingBuffer);

    void* Data;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, ImageSize, 0, &Data);
    memcpy(Data, RawImageData.Pixels, ImageSize);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    RENDERER_CORE::CreateImage(
        PhysicalDevice, 
        LogicalDevice, 
        RawImageData.Width, 
        RawImageData.Height, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        DestinationTexture.Image, 
        DestinationTexture.ImageMemory
    );

    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        RENDERER_CORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        RENDERER_CORE::CopyBufferToImage(CommandBuffer, StagingBuffer.BufferObject, DestinationTexture.Image, RawImageData.Width, RawImageData.Height);
        RENDERER_CORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    };

    RENDERER_CORE::ExecuteSingleTimeCommand(LogicalDevice, CopyCommand, CommandPool, GraphicsQueue);

    DestinationTexture.ImageView = RENDERER_CORE::CreateImageView(DestinationTexture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DestinationTexture.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    RENDERER_CORE::DestroyBuffer(LogicalDevice, StagingBuffer);
}

void RENDERER_CORE::CreateTextureImageAsync(RawImageData& RawImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, ImageData& DestinationTexture, std::mutex& Mutex)
{
    VkDeviceSize ImageSize = RawImageData.Width * RawImageData.Height * 4;

    RENDERER_CORE::Buffer StagingBuffer;
    RENDERER_CORE::CreateBuffer(PhysicalDevice, LogicalDevice, ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingBuffer);

    void* Data;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, ImageSize, 0, &Data);
    memcpy(Data, RawImageData.Pixels, ImageSize);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    RENDERER_CORE::CreateImage(
        PhysicalDevice, 
        LogicalDevice, 
        RawImageData.Width,
        RawImageData.Height,
        VK_IMAGE_TILING_OPTIMAL, 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        DestinationTexture.Image, 
        DestinationTexture.ImageMemory
    );

    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        RENDERER_CORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        RENDERER_CORE::CopyBufferToImage(CommandBuffer, StagingBuffer.BufferObject, DestinationTexture.Image, RawImageData.Width, RawImageData.Height);
        RENDERER_CORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    };

    RENDERER_CORE::ExecuteSingleTimeCommandAsync(LogicalDevice, CopyCommand, CommandPool, GraphicsQueue,Mutex);

    DestinationTexture.ImageView = RENDERER_CORE::CreateImageView(DestinationTexture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DestinationTexture.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    RENDERER_CORE::DestroyBuffer(LogicalDevice, StagingBuffer);
}

void RENDERER_CORE::ImageData::Destroy(VkDevice& LogicalDevice)
{
    if (ImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(LogicalDevice, ImageView, nullptr);
        ImageView = VK_NULL_HANDLE;
    }
    if (Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(LogicalDevice, Sampler, nullptr);
        Sampler = VK_NULL_HANDLE;
    }
    if (Image != VK_NULL_HANDLE) {
        vkDestroyImage(LogicalDevice, Image, nullptr);
        Image = VK_NULL_HANDLE;
    }
    if (ImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(LogicalDevice, ImageMemory, nullptr);
        ImageMemory = VK_NULL_HANDLE;
    }
}

void RENDERER_CORE::ImageDataMultipleSamplerViews::Destroy(VkDevice& LogicalDevice)
{
    for (auto& Sampler : Samplers)
    {
        if (Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(LogicalDevice, Sampler, nullptr);
        }
    }
    Samplers.clear();

    for (auto& ImageView : ImageViews)
    {
        if (ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(LogicalDevice, ImageView, nullptr);
        }
    }
    ImageViews.clear();

    if (Image != VK_NULL_HANDLE) {
        vkDestroyImage(LogicalDevice, Image, nullptr);
        Image = VK_NULL_HANDLE;
    }
    if (ImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(LogicalDevice, ImageMemory, nullptr);
        ImageMemory = VK_NULL_HANDLE;
    }
}

VkFormat FindSupportedFormat(VkPhysicalDevice &PhysicalDevice,const std::vector<VkFormat>& Candidates, VkImageTiling Tiling, VkFormatFeatureFlags Features)
{
    for (auto Format : Candidates)
    {
        VkFormatProperties Properties;
        vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, &Properties);

        if (Tiling == VK_IMAGE_TILING_LINEAR && (Properties.linearTilingFeatures & Features) == Features)
        {
            return Format;
        }
        else if (Tiling == VK_IMAGE_TILING_OPTIMAL && (Properties.optimalTilingFeatures & Features) == Features)
        {
            return Format;
        }

        throw std::runtime_error("Unable to find a suitable format!");
    }
}