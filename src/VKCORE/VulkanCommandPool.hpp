#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 

namespace VKCORE
{
	class CommandPool
	{
	public:
		CommandPool(uint32_t QueueFamilyIndex,VkDevice& LogicalDevice,VkCommandPoolCreateFlags Flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		void Destroy(VkDevice& LogicalDevice);
		VkCommandPool commandPool;
	private:
	};
}