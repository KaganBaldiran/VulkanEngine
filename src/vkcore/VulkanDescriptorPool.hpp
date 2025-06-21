#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
	class DescriptorPool
	{
	public:
		DescriptorPool(const std::vector<std::pair<VkDescriptorType, uint32_t>>& PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice);
		DescriptorPool() = default;
		void Create(const std::vector<std::pair<VkDescriptorType, uint32_t>>& PoolSizes, uint32_t MaxSets, VkDevice& LogicalDevice);
		void Destroy(VkDevice& LogicalDevice);
		VkDescriptorPool descriptorPool;
	private:
	};
}