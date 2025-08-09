#pragma once
#include "VulkanDescriptorPool.hpp"
#include "VulkanDescriptorSet.hpp"
#include "VulkanDescriptorSetLayout.hpp"
#include <array>

namespace RENDERER_CORE
{
	template<size_t DescriptorSetCount>
	struct Descriptor
	{
		std::array<VkDescriptorSet,DescriptorSetCount> DescriptorSets;
		DescriptorPool DescriptorPool;
		DescriptorSetLayout Layout;

		inline void Destroy(VkDevice& LogicalDevice)
		{
			this->DescriptorPool.Destroy(LogicalDevice);
			Layout.Destroy(LogicalDevice);
		}
	};
}