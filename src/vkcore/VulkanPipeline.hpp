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

    struct DescriptorSetLayoutInfo
    {
        VkDescriptorSetLayout DescriptorSet;
        uint32_t DescriptorSetCount;
    };

    struct GraphicsPipelineCreateInfo
    {
        std::vector<VkDynamicState> DynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        std::vector<ShaderModuleInfo> ShaderModules = {}; 
        std::vector<VkDescriptorSetLayout> DescriptorSetLayouts = {};
        std::vector<VkVertexInputAttributeDescription> AttributeDescriptions = {};
        std::vector<VkPushConstantRange> PushConstantRanges = {};
        VkVertexInputBindingDescription BindingDescription = {
            0,                  
            0,                  
            VK_VERTEX_INPUT_RATE_VERTEX
        };

        VkBool32 EnableDynamicRendering = VK_TRUE;
        uint32_t DynamicRenderingColorAttachmentCount = 1;
        std::vector<VkFormat> DynamicRenderingColorAttachmentsFormats = {
            VK_FORMAT_B8G8R8A8_UNORM 
        };
        VkFormat DynamicRenderingDepthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        VkRenderPass* RenderPass = nullptr; 
        VkBool32 EnableDepthWriting = VK_TRUE;
        VkBool32 EnableDepthTesting = VK_TRUE;
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        float ViewportWidth = 800.0f;  
        float ViewportHeight = 600.0f;
        VkOffset2D ScissorOffset = { 0, 0 };
        VkExtent2D ScissorExtent = { 800, 600 };
        float ViewportMinDepth = 0.0f;
        float ViewportMaxDepth = 1.0f;
    };


	class GraphicsPipeline
	{
	public:
		GraphicsPipeline(GraphicsPipelineCreateInfo &CreateInfo,VkDevice& LogicalDevice);
		GraphicsPipeline() = default;
		void Create(GraphicsPipelineCreateInfo& CreateInfo, VkDevice& LogicalDevice);
		void Destroy(VkDevice& LogicalDevice);
		VkPipeline pipeline;
		VkPipelineLayout Layout;
	private:
	};
}
