#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace RENDERER_CORE
{
	class DescriptorPool
	{
	public:
		DescriptorPool(const std::vector<std::pair<VkDescriptorType, uint32_t>>& PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice, VkDescriptorPoolCreateFlags Flags = 0);
		DescriptorPool() = default;
		void Create(const std::vector<std::pair<VkDescriptorType, uint32_t>>& PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice, VkDescriptorPoolCreateFlags Flags = 0);
		void Destroy(VkDevice& LogicalDevice);
		VkDescriptorPool Handle = VK_NULL_HANDLE;
	private:
		std::string Log;
	};
}