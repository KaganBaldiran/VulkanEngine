#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
	struct TextureData
	{
		VkImage Image;
		VkDeviceMemory ImageMemory;
		VkSampler Sampler;
		VkImageView ImageView;

		void Destroy(VkDevice& LogicalDevice);
	};

	VkFormat FindSupportedFormat(VkPhysicalDevice& PhysicalDevice,const std::vector<VkFormat>& Candidates, VkImageTiling Tiling, VkFormatFeatureFlags Features);
	void CopyBufferToImage(VkCommandBuffer& DstCommandBuffer, VkBuffer& SrcBuffer, VkImage& DstImage, uint32_t Width, uint32_t Height);
    void CreateImage(VkPhysicalDevice& PhysicaDevice, VkDevice& LogicalDevice, const uint32_t& Width, const uint32_t& Height, VkImageTiling Tiling, VkFormat Format, VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties, VkImage& Image, VkDeviceMemory& ImageMemory);
    void TransitionImageLayout(VkCommandBuffer& DstCommandBuffer, VkImage& Image, VkImageLayout OldLayout, VkImageLayout NewLayout, VkAccessFlags SrcAccessMask,
        VkAccessFlags DstAccessMask, VkPipelineStageFlags SrcStage, VkPipelineStageFlags DstStage, VkImageAspectFlags AspectMask);
    void CreateTextureSampler(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkSampler& DestinationSampler, VkFilter Filter, VkSamplerAddressMode AddressMode);
	void CreateTextureImage(const char* ImageFilePath, VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& GraphicsQueue, TextureData& DestinationTexture);
}