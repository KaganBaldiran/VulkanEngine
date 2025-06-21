#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
	//Forward Declarations
	struct Buffer;
	class DescriptorSetWriteImage;

	class DescriptorSetWriteBuffer
	{
		friend void WriteDescriptorSets(VkDevice& LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
	public:
		DescriptorSetWriteBuffer(Buffer& SourceBuffer, VkDeviceSize BufferRange,uint32_t Binding, VkDescriptorSet& DestinationSet, VkDescriptorType Type);
		DescriptorSetWriteBuffer() = default;
		void Create(
			Buffer& SourceBuffer, 
			VkDeviceSize BufferRange, 
			uint32_t Binding, 
			VkDescriptorSet& DestinationSet,
			VkDescriptorType Type
		);
	private:
		VkDescriptorBufferInfo DescriptorBufferInfo{};
		VkWriteDescriptorSet DescriptorWrite{};
	};

	class DescriptorSetWriteImage
	{
		friend void WriteDescriptorSets(VkDevice& LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
	public:
		DescriptorSetWriteImage(VkImageView& TextureImageView, VkSampler& TextureSampler, VkImageLayout TextureImageLayout, uint32_t Binding, VkDescriptorSet& DestinationSet, VkDescriptorType Type);
		DescriptorSetWriteImage() = default;
		void Create(
			VkImageView& TextureImageView, 
			VkSampler& TextureSampler,
			VkImageLayout TextureImageLayout, 
			uint32_t Binding, 
			VkDescriptorSet& DestinationSet, 
			VkDescriptorType Type
		);
	private:
		VkDescriptorImageInfo DescriptorCombinedSamplerImageInfo{};
		VkWriteDescriptorSet DescriptorWrite{};
	};

    void AllocateDescriptorSets(VkDevice& LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool& DescriptorPool, std::vector<VkDescriptorSetLayout> Layouts, std::vector<VkDescriptorSet>& DestinationSets);
    void AllocateDescriptorSets(VkDevice& LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool& DescriptorPool,VkDescriptorSetLayout Layout, std::vector<VkDescriptorSet>& DestinationSets);
    void WriteDescriptorSets(VkDevice& LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
}