#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp"
#include <vector>
#include <map>

namespace RENDERER_CORE
{
	/// <summary>
	/// Represents a Vulkan descriptor set layout, allowing the definition and management of descriptor bindings for shader resources.
	/// </summary>
	class DescriptorSetLayout
	{
		friend class DescriptorPool;
		friend class DescriptorSet;
	public:
		void AppendLayoutBinding(VkDescriptorType DescriptorType, uint32_t DescriptorCount, uint32_t Binding, VkShaderStageFlags ShaderStage);
		void CreateLayout(VkDevice& LogicalDevice, VkDescriptorSetLayoutCreateFlags Flags = 0, void* Next = nullptr);
		void Destroy(VkDevice& LogicalDevice);
		VkDescriptorSetLayout Handle = VK_NULL_HANDLE;
	private:
		std::vector<VkDescriptorSetLayoutBinding> Bindings;
	};
}