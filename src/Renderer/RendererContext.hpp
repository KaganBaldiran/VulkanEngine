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

namespace SCENE
{
    class Scene;
}

namespace RENDERER
{
	class RendererContext
	{
        friend class SCENE::Scene;
	public:
        RendererContext(bool EnableValidationLayers);
        RendererContext() = default;
        void Create(bool EnableValidationLayers);
		void Destroy();

        void WaitDeviceIdle();

        RENDERER_CORE::Window Window;
        RENDERER_CORE::Instance Instance;
        RENDERER_CORE::Surface Surface;
        RENDERER_CORE::DeviceContext DeviceContext;
        RENDERER_CORE::CommandPool CommandPool;

        RENDERER_CORE::SwapChain SwapChain;
        RENDERER_CORE::QueueFamilyIndices QueueFamilyIndices;
        bool EnableValidationLayers;

        VkFormat DepthImageFormat;

        // SceneDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Static lights to be used in the lighting pass fragment shader
        // 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Dynamic lights to be used in the lighting pass fragment shader
        // 2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Cube map to be used in the lighting pass fragment shader
        RENDERER_CORE::DescriptorSetLayout SceneDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> SceneDescriptorSetLayouts;

        // IndirectDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Model matrixes to be used in the G-buffer pass vertex shader
        // 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Indirect command buffers accessed from G-buffer pass vertex shader
        // 2: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Draw meta data buffers accessed from G-buffer pass vertex shader
        RENDERER_CORE::DescriptorSetLayout IndirectDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> IndirectDescriptorSetLayouts;

        RENDERER_CORE::DescriptorPool HDRIrenderPassDescriptorPool;
        RENDERER_CORE::DescriptorSetLayout HDRIrenderPassLayout;
        std::vector<VkDescriptorSet> HDRIrenderPassDescriptorSets;

        RENDERER_CORE::GraphicsPipeline HDRIrenderGraphicsPipeline;
        RENDERER_CORE::GraphicsPipeline HDRIconvoluteGraphicsPipeline;
        RENDERER_CORE::VertexInputDescription QuadVertexDescription{};
        RENDERER_CORE::VertexInputDescription CubeVertexDescription{};

        RENDERER_CORE::DescriptorSetLayout GbufferPassLayout;
        std::unordered_map<uint32_t,RENDERER_CORE::GraphicsPipeline> GbufferPassPassPipelines;
        RENDERER_CORE::ShaderModule GbufferVertexShaderModule;
        RENDERER_CORE::ShaderModule GbufferFragmentShaderModule;

        RENDERER_CORE::Buffer QuadVertexBuffer{};
        RENDERER_CORE::Buffer CubeVertexBuffer{};
	private:
        void CreateHDRIrenderPassResources();
        RENDERER_CORE::GraphicsPipeline* AppendGbufferPassPipeline(VkDescriptorSetLayout Layout,uint32_t MaxTextureCount);
	};
}
