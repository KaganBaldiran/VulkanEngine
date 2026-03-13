#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp"
#include <vector>
#include <functional>
#include <mutex>

namespace RENDERER_CORE
{
	VulkanResult AllocateCommandBuffers(
        VkCommandPool& CommandPool,
        VkDevice& LogicalDevice,
        std::vector<VkCommandBuffer>& DestinationCommandBuffers, 
        VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    );
	VulkanResult AllocateCommandBuffers(VkCommandPool& CommandPool,
        VkDevice& LogicalDevice,
        VkCommandBuffer* DestinationCommandBuffers,
        uint32_t CommandBufferCount,
        VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    );

    void BeginCommandBuffer(VkCommandBuffer CommandBuffer, VkCommandBufferUsageFlags Flags = 0, VkCommandBufferInheritanceInfo *InheritanceInfo = nullptr);
	void ExecuteSingleTimeCommand(VkDevice& LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue);
	void ExecuteSingleTimeCommand(VkDevice& LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandBuffer& CommandBuffer,VkFence &Fence, VkQueue& Queue);
    void ExecuteSingleTimeCommandAsync(
        VkDevice& LogicalDevice,
        std::function<void(VkCommandBuffer& CommandBuffer)> Task,
        VkCommandBuffer& CommandBuffer,
        VkFence& Fence,
        VkQueue& Queue,
        std::mutex& Mutex
    );
	void ExecuteSingleTimeCommandAsync(VkDevice& LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue,std::mutex &Mutex);
}