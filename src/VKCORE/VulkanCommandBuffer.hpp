#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp"
#include <vector>
#include <functional>

namespace VKCORE
{
	VulkanResult AllocateCommandBuffers(VkCommandPool& CommandPool, VkDevice& LogicalDevice, std::vector<VkCommandBuffer>& DestinationCommandBuffers, VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	void ExecuteSingleTimeCommand(VkDevice& LogicalDevice,std::function<void(VkCommandBuffer& CommandBuffer)> Task, VkCommandPool& Pool, VkQueue& Queue);
}