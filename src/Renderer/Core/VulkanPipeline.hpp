#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

#include "VulkanUtils.hpp" 
#include "VulkanShader.hpp"
#include "VulkanDescriptorSetLayout.hpp"

namespace RENDERER_CORE
{
    class PipelineManager;

	struct ShaderModuleInfo
	{
		ShaderModule* Module = nullptr;
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
        std::vector<ShaderModuleInfo> ShaderModules; 
        std::vector<DescriptorSetLayout> DescriptorSetLayouts;
        std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;
        std::vector<VkPushConstantRange> PushConstantRanges;
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

        //Hashes the create info, used for unique look-up on the central pipeline map
        size_t Hash() const;
        //bool operator==(const GraphicsPipelineCreateInfo& Other) const;
    };


	class GraphicsPipeline
	{
        friend class PipelineManager;
	public:
		GraphicsPipeline(GraphicsPipelineCreateInfo &CreateInfo,VkDevice& LogicalDevice,bool CalculateHash = false);
		GraphicsPipeline() = default;
		void Create(GraphicsPipelineCreateInfo& CreateInfo, VkDevice& LogicalDevice,bool CalculateHash = false);
		void Destroy(VkDevice& LogicalDevice);
		VkPipeline Handle = VK_NULL_HANDLE;
		VkPipelineLayout Layout = VK_NULL_HANDLE;

        size_t GetHash() const { return Hash; };
    private:
        size_t Hash = 0;
	};

    struct ComputePipelineCreateInfo
    {
        ShaderModule* ComputeShaderModule;
        std::vector<DescriptorSetLayout> DescriptorSetLayouts = {};
        std::vector<VkPushConstantRange> PushConstantRanges = {};

        //Hashes the create info, used for unique look-up on the central pipeline map
        size_t Hash() const;
    };

    class ComputePipeline
    {
        friend class PipelineManager;
    public:
        ComputePipeline(const ComputePipelineCreateInfo& Info,const VkDevice& LogicalDevice, bool CalculateHash = false);
        ComputePipeline() = default;
        void Create(const ComputePipelineCreateInfo& Info, const VkDevice& LogicalDevice, bool CalculateHash = false);
        void Destroy(const VkDevice& LogicalDevice);

        VkPipeline Handle = VK_NULL_HANDLE;
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        size_t GetHash() { return Hash; };
    private:
        size_t Hash = 0;
    };
}
