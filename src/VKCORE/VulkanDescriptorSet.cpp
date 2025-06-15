#include "VulkanDescriptorSet.hpp"
#include "VulkanBuffer.hpp"

void VKCORE::WriteDescriptorSets(VkDevice &LogicalDevice,std::vector<DescriptorSetWriteBuffer> BufferWrites, std::vector<DescriptorSetWriteImage> ImageWrites)
{
    std::vector<VkWriteDescriptorSet> DescriptorWrites;
    DescriptorWrites.reserve(BufferWrites.size() + ImageWrites.size());
    for (auto &BufferWrite : BufferWrites)
    {
        DescriptorWrites.push_back(BufferWrite.DescriptorWrite);
    }
    for (auto& ImageWrite : ImageWrites)
    {
        DescriptorWrites.push_back(ImageWrite.DescriptorWrite);
    }

    vkUpdateDescriptorSets(LogicalDevice, DescriptorWrites.size(), DescriptorWrites.data(), 0, nullptr);
}

void VKCORE::AllocateDescriptorSets(VkDevice &LogicalDevice,uint32_t DescriptorSetsCount,VkDescriptorPool &DescriptorPool,std::vector<VkDescriptorSetLayout> Layouts, std::vector<VkDescriptorSet> &DestinationSets)
{
    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo{};
    DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = DescriptorSetsCount;
    DescriptorSetAllocateInfo.pSetLayouts = Layouts.data();

    if (vkAllocateDescriptorSets(LogicalDevice, &DescriptorSetAllocateInfo, DestinationSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }
}

VKCORE::DescriptorSetWriteBuffer::DescriptorSetWriteBuffer(Buffer& SourceBuffer,VkDeviceSize BufferRange,uint32_t Binding,VkDescriptorSet& DestinationSet,VkDescriptorType Type)
{
    DescriptorBufferInfo.buffer = SourceBuffer.BufferObject;
    DescriptorBufferInfo.offset = 0;
    DescriptorBufferInfo.range = BufferRange;

    DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWrite.dstSet = DestinationSet;
    DescriptorWrite.dstBinding = Binding;
    DescriptorWrite.dstArrayElement = 0;
    DescriptorWrite.descriptorType = Type;
    DescriptorWrite.descriptorCount = 1;
    DescriptorWrite.pBufferInfo = &DescriptorBufferInfo;
    DescriptorWrite.pImageInfo = nullptr;
    DescriptorWrite.pTexelBufferView = nullptr;
}

VKCORE::DescriptorSetWriteImage::DescriptorSetWriteImage(VkImageView& TextureImageView,VkSampler &TextureSampler, VkImageLayout TextureImageLayout,uint32_t Binding, VkDescriptorSet& DestinationSet, VkDescriptorType Type)
{
    DescriptorCombinedSamplerImageInfo.sampler = TextureSampler;
    DescriptorCombinedSamplerImageInfo.imageView = TextureImageView;
    DescriptorCombinedSamplerImageInfo.imageLayout = TextureImageLayout;

    DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWrite.dstSet = DestinationSet;
    DescriptorWrite.dstBinding = Binding;
    DescriptorWrite.dstArrayElement = 0;
    DescriptorWrite.descriptorType = Type;
    DescriptorWrite.descriptorCount = 1;
    DescriptorWrite.pBufferInfo = nullptr;
    DescriptorWrite.pImageInfo = &DescriptorCombinedSamplerImageInfo;
    DescriptorWrite.pTexelBufferView = nullptr;
}
