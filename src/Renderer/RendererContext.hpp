#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "Core/VulkanUtils.hpp" 
#include <vector>
#include <array>

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

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "../Common/StableVector.hpp"
namespace SCENE
{
    class Scene;
    class TextureManager;
}

namespace RENDERER
{
    struct LightingPassUBOdata
    {
        glm::vec3 CameraDirection;
        float FogIntensity;
        glm::vec3 CameraPosition;
        float CameraFrustumLength;
        int StaticLightCount;
        int DynamicLightCount;
        float Time;
    };

    struct PostProcessingPassPushConstantData
    {
        glm::vec3 CameraDirection;
        float FogIntensity;
        glm::vec3 CameraPosition;
        float CameraFrustumLength;
        float Time;
    };

	class RendererContext : COMMON::Destructible
	{
        friend class SCENE::Scene;
        friend class SCENE::TextureManager;
	public:
        RendererContext(bool EnableValidationLayers);
        RendererContext() = default;
        void Create(bool EnableValidationLayers);
		void Destroy() override;

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
        // 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Indirect command buffers accessed from culling compute shader
        // 2: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Draw meta data buffers accessed from culling compute shader
        /// !TODO 3: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Culled Indirect command buffers accessed from G-buffer pass vertex shader and culling compute shader
        /// !TODO 4: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Culled Draw meta data buffers accessed from G-buffer pass vertex shader and culling compute shader
        /// !TODO 5: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Mesh visibility data buffers accessed from culling compute shader
        RENDERER_CORE::DescriptorSetLayout IndirectDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> IndirectDescriptorSetLayouts;

        // TextureIndicesDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Texture index buffers accessed from G-buffer pass fragment shader
        RENDERER_CORE::DescriptorSetLayout TextureIndicesDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> TextureIndicesDescriptorSetLayouts;

        // PostProcessDescriptorSetLayout bindings:
        // 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Main render attachment image accessed from post progressing pass fragment shader.
        // 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Depth buffer attachment image accessed from post progressing pass fragment shader.
        // 2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Normal buffer attachment image accessed from post progressing pass fragment shader.
        RENDERER_CORE::DescriptorSetLayout PostProcessDescriptorSetLayout;

        RENDERER_CORE::DescriptorPool HDRIrenderPassDescriptorPool;
        RENDERER_CORE::DescriptorSetLayout HDRIrenderPassLayout;
        std::vector<VkDescriptorSet> HDRIrenderPassDescriptorSets;

        RENDERER_CORE::GraphicsPipeline HDRIrenderGraphicsPipeline;
        RENDERER_CORE::GraphicsPipeline HDRIconvoluteGraphicsPipeline;
        RENDERER_CORE::VertexInputDescription QuadVertexDescription{};
        RENDERER_CORE::VertexInputDescription CubeVertexDescription{};

        //COMMON::StableVector<RENDERER_CORE::GraphicsPipeline> GraphicsPipelines;
        //COMMON::StableVector<RENDERER_CORE::ShaderModule> ShaderModules;

        //First two are g-buffer pass and the third one is shading pass pipeline
        RENDERER_CORE::ShaderModule GbufferVertexShaderModule;
        RENDERER_CORE::ShaderModule GbufferFragmentShaderModule;

        RENDERER_CORE::ShaderModule LightingVertexShaderModule;
        RENDERER_CORE::ShaderModule LightingFragmentShaderModule;
        RENDERER_CORE::GraphicsPipelineCreateInfo LatestShadingPassPipelineCreateInfo;

        RENDERER_CORE::ShaderModule PostProcessingFragmentShaderModule;
        RENDERER_CORE::GraphicsPipeline PostProcessingGraphicsPipeline;
        RENDERER_CORE::GraphicsPipelineCreateInfo LatestPostProcessPassPipelineCreateInfo;

        std::unordered_map<uint32_t,std::array<RENDERER_CORE::GraphicsPipeline,3>> TextureDescriptorPipelines;
        //RENDERER_CORE::GraphicsPipeline DeferredLightingPassGraphicsPipeline;
        RENDERER_CORE::DescriptorSetLayout LightingPassLayout;

        RENDERER_CORE::Buffer QuadVertexBuffer{};
        RENDERER_CORE::Buffer CubeVertexBuffer{};

        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> SingleTimeCommandBuffers;
        RENDERER_CORE::Fence SingleTimeCommandFence;
	private:
        void CreateHDRIrenderPassResources();
        std::array<RENDERER_CORE::GraphicsPipeline,3>* CreateTextureDescriptorPipelines(VkDescriptorSetLayout Layout,uint32_t MaxTextureCount);
        void CreatePostProcessingPassPipeline();
	};
}
