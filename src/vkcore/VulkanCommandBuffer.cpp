#include "VulkanCommandBuffer.hpp"

VKCORE::VulkanResult VKCORE::AllocateCommandBuffers(VkCommandPool& CommandPool,VkDevice& LogicalDevice,std::vector<VkCommandBuffer> &DestinationCommandBuffers,VkCommandBufferLevel Level)
{ 
    VkCommandBufferAllocateInfo AllocCreateInfo{};
    AllocCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocCreateInfo.commandPool = CommandPool;
    AllocCreateInfo.level = Level;
    AllocCreateInfo.commandBufferCount = static_cast<uint32_t>(DestinationCommandBuffers.size());

    if (vkAllocateCommandBuffers(LogicalDevice, &AllocCreateInfo, DestinationCommandBuffers.data()) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE,"Failed to allocate command buffers" };
    }
    return VULKAN_SUCCESS;
}

void VKCORE::ExecuteSingleTimeCommand(VkDevice &LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue)
{
    VkFence Fence{};
    VkFenceCreateInfo FenceCreateInfo{};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    if (vkCreateFence(LogicalDevice,&FenceCreateInfo,nullptr,&Fence) != VK_SUCCESS)
    {
        throw std::runtime_error("Error creating a single time fence!");
    }

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo{};
    CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool = Pool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer SingleUseCommandBuffer;
    vkAllocateCommandBuffers(LogicalDevice, &CommandBufferAllocateInfo, &SingleUseCommandBuffer);

    VkCommandBufferBeginInfo CommandBufferBeginInfo{};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(SingleUseCommandBuffer, &CommandBufferBeginInfo);

    Task(SingleUseCommandBuffer);

    vkEndCommandBuffer(SingleUseCommandBuffer);

    VkSubmitInfo CommandBufferSubmitInfo{};
    CommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    CommandBufferSubmitInfo.commandBufferCount = 1;
    CommandBufferSubmitInfo.pCommandBuffers = &SingleUseCommandBuffer;

    vkQueueSubmit(Queue, 1, &CommandBufferSubmitInfo,Fence);
    vkWaitForFences(LogicalDevice, 1, &Fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(LogicalDevice, Fence, nullptr);
    vkFreeCommandBuffers(LogicalDevice, Pool, 1, &SingleUseCommandBuffer);
}
