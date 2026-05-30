#include "VulkanCommandBuffer.hpp"

RENDERER_CORE::VulkanResult RENDERER_CORE::AllocateCommandBuffers(VkCommandPool& CommandPool,VkDevice& LogicalDevice,std::vector<VkCommandBuffer> &DestinationCommandBuffers,VkCommandBufferLevel Level)
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

RENDERER_CORE::VulkanResult RENDERER_CORE::AllocateCommandBuffers(
    VkCommandPool& CommandPool,
    VkDevice& LogicalDevice,
    VkCommandBuffer* DestinationCommandBuffers,
    uint32_t CommandBufferCount, 
    VkCommandBufferLevel Level
)
{
    VkCommandBufferAllocateInfo AllocCreateInfo{};
    AllocCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocCreateInfo.commandPool = CommandPool;
    AllocCreateInfo.level = Level;
    AllocCreateInfo.commandBufferCount = CommandBufferCount;

    if (vkAllocateCommandBuffers(LogicalDevice, &AllocCreateInfo, DestinationCommandBuffers) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE,"Failed to allocate command buffers" };
    }
    return VULKAN_SUCCESS;
}

void RENDERER_CORE::BeginCommandBuffer(VkCommandBuffer CommandBuffer,VkCommandBufferUsageFlags Flags,VkCommandBufferInheritanceInfo *InheritanceInfo)
{
    VkCommandBufferBeginInfo CommandBufferBeginInfo{};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = Flags;
    CommandBufferBeginInfo.pInheritanceInfo = InheritanceInfo;

    if (vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void RENDERER_CORE::EndCommandBuffer(VkCommandBuffer CommandBuffer)
{
    if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end a command buffer");
    }
}

void RENDERER_CORE::ExecuteSingleTimeCommand(VkDevice &LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue)
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

void RENDERER_CORE::ExecuteSingleTimeCommand(
    VkDevice& LogicalDevice, 
    std::function<void(VkCommandBuffer& CommandBuffer)> Task, 
    VkCommandBuffer& CommandBuffer, 
    VkFence& Fence, 
    VkQueue& Queue
)
{
    VkCommandBufferBeginInfo CommandBufferBeginInfo{};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);
    Task(CommandBuffer);
    vkEndCommandBuffer(CommandBuffer);

    VkSubmitInfo CommandBufferSubmitInfo{};
    CommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    CommandBufferSubmitInfo.commandBufferCount = 1;
    CommandBufferSubmitInfo.pCommandBuffers = &CommandBuffer;

    vkQueueSubmit(Queue, 1, &CommandBufferSubmitInfo, Fence);
    vkWaitForFences(LogicalDevice, 1, &Fence, VK_TRUE, UINT64_MAX);
    vkResetFences(LogicalDevice, 1, &Fence);
    vkResetCommandBuffer(CommandBuffer, 0);
}

void RENDERER_CORE::ExecuteSingleTimeCommandAsync(
    VkDevice& LogicalDevice,
    std::function<void(VkCommandBuffer& CommandBuffer)> Task,
    VkCommandBuffer& CommandBuffer,
    VkFence& Fence,
    VkQueue& Queue,
    std::mutex& Mutex
)
{
    std::unique_lock<std::mutex> lock(Mutex);
    VkCommandBufferBeginInfo CommandBufferBeginInfo{};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);
    Task(CommandBuffer);
    vkEndCommandBuffer(CommandBuffer);

    VkSubmitInfo CommandBufferSubmitInfo{};
    CommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    CommandBufferSubmitInfo.commandBufferCount = 1;
    CommandBufferSubmitInfo.pCommandBuffers = &CommandBuffer;

    vkQueueSubmit(Queue, 1, &CommandBufferSubmitInfo, Fence);
    vkWaitForFences(LogicalDevice, 1, &Fence, VK_TRUE, UINT64_MAX);
    vkResetFences(LogicalDevice, 1, &Fence);
    vkResetCommandBuffer(CommandBuffer, 0);
}

void RENDERER_CORE::ExecuteSingleTimeCommandAsync(VkDevice& LogicalDevice, std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue, std::mutex& Mutex)
{
    VkFence Fence{};
    VkFenceCreateInfo FenceCreateInfo{};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (vkCreateFence(LogicalDevice, &FenceCreateInfo, nullptr, &Fence) != VK_SUCCESS)
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

    {
        std::unique_lock<std::mutex> lock(Mutex);
        vkQueueSubmit(Queue, 1, &CommandBufferSubmitInfo, Fence);
    }
    vkWaitForFences(LogicalDevice, 1, &Fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(LogicalDevice, Fence, nullptr);
    vkFreeCommandBuffers(LogicalDevice, Pool, 1, &SingleUseCommandBuffer);
}
