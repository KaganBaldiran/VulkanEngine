#include "VulkanCommandPool.hpp"

RENDERER_CORE::CommandPool::CommandPool(uint32_t QueueFamilyIndex,VkDevice &LogicalDevice, VkCommandPoolCreateFlags Flags)
{
    Create(QueueFamilyIndex, LogicalDevice, Flags);
}

void RENDERER_CORE::CommandPool::Create(uint32_t QueueFamilyIndex, VkDevice& LogicalDevice, VkCommandPoolCreateFlags Flags)
{
    VkCommandPoolCreateInfo CommandPoolCreateInfo{};
    CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCreateInfo.flags = Flags;
    CommandPoolCreateInfo.queueFamilyIndex = QueueFamilyIndex;

    VkResult result = vkCreateCommandPool(LogicalDevice, &CommandPoolCreateInfo, nullptr, &Handle);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed with error: " + std::to_string(result));
    }
}

void RENDERER_CORE::CommandPool::Destroy(VkDevice &LogicalDevice)
{
    if (Handle == VK_NULL_HANDLE) return;
    vkDestroyCommandPool(LogicalDevice,Handle,nullptr);
}

