#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>
#include "VulkanShader.hpp"

namespace VKCORE
{
	struct ShaderModuleInfo
	{
		ShaderModule* Module;
		VkShaderStageFlagBits Usage;
	};

	struct GraphicsPipelineCreateInfo
	{
		std::vector<VkDynamicState> DynamicStates;
		std::vector<ShaderModuleInfo> ShaderModules;
		std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
		std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;
		VkVertexInputBindingDescription BindingDescription;

		VkBool32 EnableDynamicRendering;
		uint32_t DynamicRenderingColorAttachmentCount;
		std::vector<VkFormat> DynamicRenderingColorAttachmentsFormats;
		VkFormat DynamicRenderingDepthAttachmentFormat;
		VkRenderPass* RenderPass;

		VkPrimitiveTopology Topology;
		float ViewportWidth;
		float ViewportHeight;
		VkOffset2D ScissorOffset;
		VkExtent2D ScissorExtent;
		float ViewportMinDepth;
		float ViewportMaxDepth;
	};

	class GraphicsPipeline
	{
	public:
		GraphicsPipeline(GraphicsPipelineCreateInfo &CreateInfo,VkDevice& LogicalDevice);
		void Destroy(VkDevice& LogicalDevice);
		VkPipeline pipeline;
		VkPipelineLayout Layout;
	private:
	};
}
