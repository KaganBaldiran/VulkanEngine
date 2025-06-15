#include "VulkanCommandPool.hpp"

VKCORE::CommandPool::CommandPool(uint32_t QueueFamilyIndex,VkDevice &LogicalDevice, VkCommandPoolCreateFlags Flags)
{
    VkCommandPoolCreateInfo CommandPoolCreateInfo{};
    CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCreateInfo.flags = Flags;
    CommandPoolCreateInfo.queueFamilyIndex = QueueFamilyIndex;

    VkResult result = vkCreateCommandPool(LogicalDevice, &CommandPoolCreateInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed with error: " + std::to_string(result));
    }
}

void VKCORE::CommandPool::Destroy(VkDevice &LogicalDevice)
{
    if (commandPool == VK_NULL_HANDLE) return;
    vkDestroyCommandPool(LogicalDevice,commandPool,nullptr);
}

