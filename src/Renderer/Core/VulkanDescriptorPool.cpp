#include "VulkanDescriptorPool.hpp"
#include "VulkanDescriptorSetLayout.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

RENDERER_CORE::DescriptorPool::DescriptorPool(const std::vector<std::pair<VkDescriptorType,uint32_t>> &PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice, VkDescriptorPoolCreateFlags Flags)
{
    Create(PoolSizes, MaxSets, LogicalDevice,Flags);
}

void RENDERER_CORE::DescriptorPool::Create(const std::vector<std::pair<VkDescriptorType, uint32_t>>& PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice,VkDescriptorPoolCreateFlags Flags)
{
    if (PoolSizes.empty() || MaxSets == 0)
        throw std::runtime_error("Descriptor pool sizes or max sets must be non-zero");

    std::vector<VkDescriptorPoolSize> Sizes(PoolSizes.size());
    for (size_t i = 0; i < PoolSizes.size(); i++)
    {
        Sizes[i].type = PoolSizes[i].first;
        Sizes[i].descriptorCount = PoolSizes[i].second;
        Log += "(" + std::string(string_VkDescriptorType(Sizes[i].type)) + "," + std::to_string(Sizes[i].descriptorCount) + ")";
    }

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo{};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.poolSizeCount = Sizes.size();
    DescriptorPoolCreateInfo.pPoolSizes = Sizes.data();
    DescriptorPoolCreateInfo.maxSets = MaxSets;
    DescriptorPoolCreateInfo.flags = Flags;

    if (vkCreateDescriptorPool(LogicalDevice, &DescriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_ERROR, std::string("Failed creating descriptor pool [" + Log + "]."));
        throw std::runtime_error("Failed to create a descriptor pool");
    }
    LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_INFO, std::string("Created descriptor pool [" + Log + "]."));
}

void RENDERER_CORE::DescriptorPool::Destroy(VkDevice& LogicalDevice)
{
    if (descriptorPool)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_INFO, std::string("Destroyed descriptor pool [" + Log + "]."));
        Log.clear();
        vkDestroyDescriptorPool(LogicalDevice, this->descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}
