#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../vkcore/VulkanUtils.hpp" 
#include <vector>

#include "../vkcore/VulkanCommandPool.hpp"
#include "../vkcore/VulkanCommandBuffer.hpp"
#include "../vkcore/VulkanDescriptorPool.hpp"
#include "../vkcore/VulkanDescriptorSetLayout.hpp"
#include "../vkcore/VulkanDescriptorSet.hpp"
#include "../vkcore/VulkanDevice.hpp"
#include "../vkcore/VulkanPipeline.hpp"
#include "../vkcore/VulkanImage.hpp"
#include "../vkcore/VulkanImageView.hpp"
#include "../vkcore/VulkanWindow.hpp"
#include "../vkcore/VulkanDevice.hpp"
#include "../vkcore/VulkanSwapChain.hpp"
#include "../vkcore/VulkanInstance.hpp"
#include "../vkcore/VulkanRender.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "../vkcore/VertexInputDescription.hpp"

const int MAX_FRAMES_IN_FLIGHT = 3;

namespace VKSCENE
{
    class Scene;
}

namespace VKAPP
{
	class RendererContext
	{
        friend class VKSCENE::Scene;
	public:
        RendererContext(bool EnableValidationLayers);
        RendererContext() = default;
        void Create(bool EnableValidationLayers);
		void Destroy();

        void WaitDeviceIdle();

        VKCORE::Window Window;
        VKCORE::Instance Instance;
        VKCORE::Surface Surface;
        VKCORE::DeviceContext DeviceContext;
        VKCORE::CommandPool CommandPool;

        VKCORE::SwapChain SwapChain;
        VKCORE::QueueFamilyIndices QueueFamilyIndices;
        bool EnableValidationLayers;

        VkFormat DepthImageFormat;

        VKCORE::DescriptorSetLayout SceneDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> SceneDescriptorSetLayouts;

        VKCORE::DescriptorSetLayout IndirectDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> IndirectDescriptorSetLayouts;

        VKCORE::DescriptorPool HDRIrenderPassDescriptorPool;
        VKCORE::DescriptorSetLayout HDRIrenderPassLayout;
        std::vector<VkDescriptorSet> HDRIrenderPassDescriptorSets;

        VKCORE::GraphicsPipeline HDRIrenderGraphicsPipeline;
        VKCORE::GraphicsPipeline HDRIconvoluteGraphicsPipeline;
        VKCORE::VertexInputDescription QuadVertexDescription{};
        VKCORE::VertexInputDescription CubeVertexDescription{};

        VKCORE::DescriptorSetLayout GbufferPassLayout;
        std::unordered_map<uint32_t,VKCORE::GraphicsPipeline> GbufferPassPassPipelines;
        VKCORE::ShaderModule GbufferVertexShaderModule;
        VKCORE::ShaderModule GbufferFragmentShaderModule;

        VKCORE::Buffer QuadVertexBuffer{};
        VKCORE::Buffer CubeVertexBuffer{};
	private:
        void CreateHDRIrenderPassResources();
        VKCORE::GraphicsPipeline* AppendGbufferPassPipeline(VkDescriptorSetLayout Layout,uint32_t MaxTextureCount);
	};
}
