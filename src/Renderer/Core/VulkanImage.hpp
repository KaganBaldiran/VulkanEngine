#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <mutex>

#include "VulkanSynchoronization.hpp"
#include "VulkanUtils.hpp" 

namespace RENDERER_CORE
{
	struct TextureData
	{
		VkImage Image = VK_NULL_HANDLE;
		VkDeviceMemory ImageMemory = VK_NULL_HANDLE;
		VkSampler Sampler = VK_NULL_HANDLE;
		VkImageView ImageView = VK_NULL_HANDLE;
		ImageBarrierState BarrierState{};
		void Destroy(VkDevice& LogicalDevice);
	};

	struct TextureDataMultipleSamplerViews
	{
		VkImage Image = VK_NULL_HANDLE;
		VkDeviceMemory ImageMemory = VK_NULL_HANDLE;
		std::vector<VkSampler> Samplers;
		std::vector<VkImageView> ImageViews;

		void Destroy(VkDevice& LogicalDevice);
	};

	struct RawImageData
	{
		int Width;
		int Height;
		int ChannelCount;
		unsigned char* Pixels;
	};

	int ReadTexture(const char* FileName, RENDERER_CORE::RawImageData& DestinationImageData);

	VkFormat FindSupportedFormat(VkPhysicalDevice& PhysicalDevice,const std::vector<VkFormat>& Candidates, VkImageTiling Tiling, VkFormatFeatureFlags Features);
	void CopyBufferToImage(VkCommandBuffer& DstCommandBuffer, VkBuffer& SrcBuffer, VkImage& DstImage, uint32_t Width, uint32_t Height);
	void CreateImage(
		VkPhysicalDevice& PhysicalDevice,
		VkDevice& LogicalDevice,
		const uint32_t& Width,
		const uint32_t& Height,
		VkImageTiling Tiling,
		VkFormat Format,
		VkImageUsageFlags Usage,
		VkMemoryPropertyFlags Properties,
		VkImage& Image,
		VkDeviceMemory& ImageMemory,
		uint32_t LayerCount = 1,
		VkImageCreateFlags Flags = 0,
		VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT
	);

	void BlitImage(
		VkCommandBuffer& CommandBuffer,
		VkImage SrcImage,
		VkImage DstImage,
		glm::ivec3 SrcOffset,
		glm::ivec3 SrcSizes,
		glm::ivec3 DstOffset,
		glm::ivec3 DstSizes,
		VkImageAspectFlags SrcAspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		uint32_t SrcMipLevel = 0,
		uint32_t SrcBaseArrayLayer = 0,
		uint32_t SrcLayerCount = 1,
		VkImageAspectFlags DstAspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		uint32_t DstMipLevel = 0,
		uint32_t DstBaseArrayLayer = 0,
		uint32_t DstLayerCount = 1,
		VkFilter Filter = VK_FILTER_LINEAR
	);
    void TransitionImageLayout(VkCommandBuffer& DstCommandBuffer, VkImage& Image, VkImageLayout OldLayout, VkImageLayout NewLayout, VkAccessFlags SrcAccessMask,
        VkAccessFlags DstAccessMask, VkPipelineStageFlags SrcStage, VkPipelineStageFlags DstStage, VkImageAspectFlags AspectMask, uint32_t LayerCount = 1,uint32_t BaseArrayLayer = 0);
    void CreateTextureSampler(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkSampler& DestinationSampler, VkFilter Filter, VkSamplerAddressMode AddressMode,VkSamplerMipmapMode MipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR);
	void CreateTextureImage(const char* ImageFilePath, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture);
	void CreateTextureImage(RawImageData &ImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture);
	void CreateTextureImageAsync(RawImageData &ImageData, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture,std::mutex &Mutex);
}