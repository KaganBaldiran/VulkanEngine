#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

#include "VulkanUtils.hpp" 

namespace RENDERER_CORE
{
	//Forward Declarations
	struct Buffer;
	class DescriptorSetWriteImage;

	class DescriptorSetWriteBuffer
	{
		friend void WriteDescriptorSets(VkDevice LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
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

		DescriptorSetWriteBuffer(const DescriptorSetWriteBuffer& Other);
		DescriptorSetWriteBuffer(DescriptorSetWriteBuffer&& Other) noexcept;
	private:
		VkDescriptorBufferInfo DescriptorBufferInfo{};
		VkWriteDescriptorSet DescriptorWrite{};
	};

	class DescriptorSetWriteImage
	{
		friend void WriteDescriptorSets(VkDevice LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
	public:
		DescriptorSetWriteImage(
			VkImageView TextureImageView, 
			VkSampler TextureSampler, 
			VkImageLayout TextureImageLayout, 
			uint32_t Binding, 
			VkDescriptorSet DestinationSet, 
			VkDescriptorType Type,
			uint32_t DstArrayElement = 0,
			uint32_t DescriptorCount = 1
		);
		DescriptorSetWriteImage() = default;
		void Create(
			VkImageView TextureImageView, 
			VkSampler TextureSampler,
			VkImageLayout TextureImageLayout, 
			uint32_t Binding, 
			VkDescriptorSet DestinationSet, 
			VkDescriptorType Type,
			uint32_t DstArrayElement = 0,
			uint32_t DescriptorCount = 1
		);

		DescriptorSetWriteImage(const DescriptorSetWriteImage &Other);
		DescriptorSetWriteImage(DescriptorSetWriteImage &&Other) noexcept;

		VkWriteDescriptorSet DescriptorWrite{};
		VkDescriptorImageInfo DescriptorCombinedSamplerImageInfo{};
	private:
	};

    void AllocateDescriptorSets(VkDevice LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool DescriptorPool, std::vector<VkDescriptorSetLayout> Layouts, std::vector<VkDescriptorSet>& DestinationSets);
    void AllocateDescriptorSets(VkDevice LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool DescriptorPool,VkDescriptorSetLayout Layout, std::vector<VkDescriptorSet>& DestinationSets);
	void AllocateDescriptorSets(VkDevice LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool DescriptorPool, VkDescriptorSetLayout Layout, VkDescriptorSet* DestinationSets);
    void AllocateDescriptorSets(VkDevice LogicalDevice, uint32_t DescriptorSetsCount, VkDescriptorPool DescriptorPool,VkDescriptorSetLayout Layout, VkDescriptorSet& DestinationSet);
    void WriteDescriptorSets(VkDevice LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites);
}