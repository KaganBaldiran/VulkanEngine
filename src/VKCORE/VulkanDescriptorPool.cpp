#include "VulkanDescriptorPool.hpp"
#include "VulkanDescriptorSetLayout.hpp"

VKCORE::DescriptorPool::DescriptorPool(const std::vector<std::pair<VkDescriptorType,uint32_t>> &PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice)
{
    std::vector<VkDescriptorPoolSize> Sizes(PoolSizes.size());
    for (size_t i = 0; i < PoolSizes.size(); i++)
    {
        Sizes[i].type = PoolSizes[i].first;
        Sizes[i].descriptorCount = PoolSizes[i].second;
    }
    
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo{};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.poolSizeCount = Sizes.size();
    DescriptorPoolCreateInfo.pPoolSizes = Sizes.data();
    DescriptorPoolCreateInfo.maxSets = MaxSets;

    if (vkCreateDescriptorPool(LogicalDevice, &DescriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a descriptor pool");
    }
}

void VKCORE::DescriptorPool::Destroy(VkDevice& LogicalDevice)
{
    vkDestroyDescriptorPool(LogicalDevice, this->descriptorPool, nullptr);
}
