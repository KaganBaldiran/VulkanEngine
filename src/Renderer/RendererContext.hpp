#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "Core/VulkanUtils.hpp" 
#include <vector>

#include "Core/VulkanCommandPool.hpp"
#include "Core/VulkanCommandBuffer.hpp"
#include "Core/VulkanDescriptorPool.hpp"
#include "Core/VulkanDescriptorSetLayout.hpp"
#include "Core/VulkanDescriptorSet.hpp"
#include "Core/VulkanDevice.hpp"
#include "Core/VulkanPipeline.hpp"
#include "Core/VulkanImage.hpp"
#include "Core/VulkanImageView.hpp"
#include "Core/VulkanWindow.hpp"
#include "Core/VulkanDevice.hpp"
#include "Core/VulkanSwapChain.hpp"
#include "Core/VulkanInstance.hpp"
#include "Core/VulkanRender.hpp"
#include "Core/VulkanBuffer.hpp"
#include "Core/VertexInputDescription.hpp"
#include "../Common/CommonDefinitions.hpp"

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

        // SceneDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Static lights to be used in the lighting pass fragment shader
        // 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Dynamic lights to be used in the lighting pass fragment shader
        // 2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Cube map to be used in the lighting pass fragment shader
        VKCORE::DescriptorSetLayout SceneDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> SceneDescriptorSetLayouts;

        // IndirectDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Model matrixes to be used in the G-buffer pass vertex shader
        // 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Indirect command buffers accessed from G-buffer pass vertex shader
        // 2: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Draw meta data buffers accessed from G-buffer pass vertex shader
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
