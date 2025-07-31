#pragma once
#include "VulkanImage.hpp"
#include "VulkanBuffer.hpp"
//#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"
#include "VulkanImageView.hpp"
#include "VulkanCommandBuffer.hpp"

int VKCORE::ReadTexture(const char* FileName, VKCORE::RawImageData& DestinationImageData)
{
    DestinationImageData.Pixels = stbi_load(FileName, &DestinationImageData.Width, &DestinationImageData.Height, &DestinationImageData.ChannelCount, STBI_rgb_alpha);
    if (!DestinationImageData.Pixels)
    {
        std::cout << "Unable to load the image(" + std::string(FileName) + ")" << std::endl;
        return -1;
    }
    return 0;
}

VkFormat VKCORE::FindSupportedFormat(VkPhysicalDevice &PhysicalDevice,const std::vector<VkFormat>& Candidates, VkImageTiling Tiling, VkFormatFeatureFlags Features)
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

void VKCORE::CreateImage(
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
    VkImageCreateFlags Flags
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
    ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.flags = Flags;

    if (vkCreateImage(LogicalDevice, &ImageCreateInfo, nullptr, &Image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements ImageMemoryRequirements;
    vkGetImageMemoryRequirements(LogicalDevice, Image, &ImageMemoryRequirements);

    VkMemoryAllocateInfo ImageMemoryAllocationInfo{};
    ImageMemoryAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ImageMemoryAllocationInfo.allocationSize = ImageMemoryRequirements.size;
    ImageMemoryAllocationInfo.memoryTypeIndex = VKCORE::FindMemoryType(PhysicalDevice,ImageMemoryRequirements.memoryTypeBits, Properties);

    if (vkAllocateMemory(LogicalDevice, &ImageMemoryAllocationInfo, nullptr, &ImageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for the image");
    }

    vkBindImageMemory(LogicalDevice, Image, ImageMemory, 0);
}

void VKCORE::BlitImage(
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

void VKCORE::TransitionImageLayout(VkCommandBuffer& DstCommandBuffer, VkImage& Image, VkImageLayout OldLayout, VkImageLayout NewLayout, VkAccessFlags SrcAccessMask,
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

void VKCORE::CreateTextureSampler(VkPhysicalDevice& PhysicalDevice,VkDevice &LogicalDevice,VkSampler &DestinationSampler,VkFilter Filter,VkSamplerAddressMode AddressMode)
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
    SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    SamplerCreateInfo.mipLodBias = 0.0f;
    SamplerCreateInfo.minLod = 0.0f;
    SamplerCreateInfo.maxLod = 0.0f;

    if (vkCreateSampler(LogicalDevice, &SamplerCreateInfo, nullptr, &DestinationSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

void VKCORE::CopyBufferToImage(VkCommandBuffer& DstCommandBuffer, VkBuffer& SrcBuffer, VkImage& DstImage, uint32_t Width, uint32_t Height)
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

void VKCORE::CreateTextureImage(const char* ImageFilePath,VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice,VkCommandPool &CommandPool,VkQueue &GraphicsQueue,TextureData &DestinationTexture)
{
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

void VKCORE::CreateTextureImage(RawImageData& ImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture)
{
    VkDeviceSize ImageSize = ImageData.Width * ImageData.Height * 4;

    VKCORE::Buffer StagingBuffer;
    VKCORE::CreateBuffer(PhysicalDevice, LogicalDevice, ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingBuffer);

    void* Data;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, ImageSize, 0, &Data);
    memcpy(Data, ImageData.Pixels, ImageSize);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, ImageData.Width, ImageData.Height, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DestinationTexture.Image, DestinationTexture.ImageMemory);

    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        VKCORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        VKCORE::CopyBufferToImage(CommandBuffer, StagingBuffer.BufferObject, DestinationTexture.Image, ImageData.Width, ImageData.Height);
        VKCORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        };

    VKCORE::ExecuteSingleTimeCommand(LogicalDevice, CopyCommand, CommandPool, GraphicsQueue);

    DestinationTexture.ImageView = VKCORE::CreateImageView(DestinationTexture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DestinationTexture.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    StagingBuffer.Destroy(LogicalDevice);
}

void VKCORE::CreateTextureImageAsync(RawImageData& ImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture, std::mutex& Mutex)
{
    VkDeviceSize ImageSize = ImageData.Width * ImageData.Height * 4;

    VKCORE::Buffer StagingBuffer;
    VKCORE::CreateBuffer(PhysicalDevice, LogicalDevice, ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, StagingBuffer);

    void* Data;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, ImageSize, 0, &Data);
    memcpy(Data, ImageData.Pixels, ImageSize);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, ImageData.Width, ImageData.Height, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DestinationTexture.Image, DestinationTexture.ImageMemory);

    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        VKCORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        VKCORE::CopyBufferToImage(CommandBuffer, StagingBuffer.BufferObject, DestinationTexture.Image, ImageData.Width, ImageData.Height);
        VKCORE::TransitionImageLayout(CommandBuffer, DestinationTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    };

    VKCORE::ExecuteSingleTimeCommandAsync(LogicalDevice, CopyCommand, CommandPool, GraphicsQueue,Mutex);

    DestinationTexture.ImageView = VKCORE::CreateImageView(DestinationTexture.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
    VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DestinationTexture.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    StagingBuffer.Destroy(LogicalDevice);
}

void VKCORE::TextureData::Destroy(VkDevice& LogicalDevice)
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