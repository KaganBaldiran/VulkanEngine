#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include <array>
#include <unordered_map>

#include "Core/VulkanUtils.hpp" 
#include "Core/VulkanCommandPool.hpp"
#include "Core/VulkanCommandBuffer.hpp"
#include "Core/VulkanDescriptorPool.hpp"
#include "Core/VulkanDescriptorSetLayout.hpp"
#include "Core/VulkanDescriptorSet.hpp"
#include "Core/VulkanDescriptor.hpp"
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
#include "Core/PipelineManager.hpp"
#include "Core/ShaderManager.hpp"

#include "RenderPipeline.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/StableVector.hpp"

namespace SCENE
{
    class Scene;
}

namespace RENDERER
{
    class TextureManager;
    class Renderer;
    class DeferredRenderPipeline;

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

    struct GPUmemoryStats
    {
        size_t TotalBudgetBytes = 0;
        size_t TotalUsedBytes = 0;
        float UsageRate = 0.0f;
    };

	class RendererContext : COMMON::Destructible
	{
        friend class SCENE::Scene;
        friend class TextureManager;
        friend class Renderer;
        friend class DeferredRenderPipeline;
	public:
        RendererContext(
            uint32_t WindowWidth,
            uint32_t WindowHeight,
            const char* WindowName,
            bool EnableValidationLayers
        );
        RendererContext() = default;
        void Create(
            uint32_t WindowWidth,
            uint32_t WindowHeight,
            const char* WindowName,
            bool EnableValidationLayers
        );
		void Destroy() override;
        void WaitDeviceIdle();
        GPUmemoryStats QueryMemoryStats();

        RENDERER_CORE::Window Window;
        RENDERER_CORE::Instance Instance;
        RENDERER_CORE::Surface Surface;
        RENDERER_CORE::DeviceContext DeviceContext;
        RENDERER_CORE::CommandPool CommandPool;

        RENDERER_CORE::SwapChain SwapChain;
        RENDERER_CORE::QueueFamilyIndices QueueFamilyIndices;
        bool EnableValidationLayers;
        VkFormat DepthImageFormat;

        RENDERER_CORE::Buffer QuadVertexBuffer{};
        RENDERER_CORE::Buffer CubeVertexBuffer{};

        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> SingleTimeCommandBuffers;
        RENDERER_CORE::Fence SingleTimeCommandFence;

        RENDERER_CORE::VertexInputDescription QuadVertexDescription{};
        RENDERER_CORE::VertexInputDescription CubeVertexDescription{};

        RENDERER_CORE::PipelineManager PipelineManager;
        RENDERER_CORE::ShaderManager ShaderManager;

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
        //TODO 3: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Culled Indirect command buffers accessed from G-buffer pass vertex shader and culling compute shader
        //TODO 4: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Culled Draw meta data buffers accessed from G-buffer pass vertex shader and culling compute shader
        //TODO 5: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Mesh visibility data buffers accessed from culling compute shader
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

        RENDERER_CORE::DescriptorSetLayout LightingPassLayout;

        // 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Texture array
        std::array<RENDERER_CORE::Descriptor<1>, MAX_FRAMES_IN_FLIGHT> TexturesDescriptors;
        std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> TextureDescriptorUpperBounds;
        std::array<RENDERER_CORE::VirtualArenaAllocator, MAX_FRAMES_IN_FLIGHT> TextureDescriptorIndexAllocators;

        //Storage structure to store constant pipeline indexes.
        struct DefaultPipelineIndices
        {
            size_t HDRIrender;
            size_t HDRIconvolute;
            size_t PostProcessing;
            std::array<size_t,MAX_FRAMES_IN_FLIGHT> GbufferDepthDisabled;
            std::array<size_t, MAX_FRAMES_IN_FLIGHT> GbufferDepthEnabled;
            std::array<size_t, MAX_FRAMES_IN_FLIGHT> DeferredShading;

            DefaultPipelineIndices()
            {
                HDRIrender = std::numeric_limits<size_t>::max();
                HDRIconvolute = std::numeric_limits<size_t>::max();
                PostProcessing = std::numeric_limits<size_t>::max();
                GbufferDepthDisabled.fill(std::numeric_limits<size_t>::max());
                GbufferDepthEnabled.fill(std::numeric_limits<size_t>::max());
                DeferredShading.fill(std::numeric_limits<size_t>::max());
            }
        };
        DefaultPipelineIndices DefaultPipelines;
	private:
        //Creates texture descriptors. If the provided descriptor count exceeds the current free descriptor count, it recreates the descriptors.
        //In case the descriptors are recreated then the pipelines that reference the prior descriptors are also recreated.
        bool CreateTextureDescriptors(uint32_t DescriptorCount, uint32_t FrameIndex,bool DestroyPrevious = false);
        void DestroyMeshTextureDescriptors();

        void CreateHDRIrenderPassResources();
        void CreateTextureDescriptorPipelines(RENDERER_CORE::DescriptorSetLayout& Layout, uint32_t MaxTextureCount, uint32_t FrameIndex);
        //Recreates texture descriptor dependent custom pipelines. 
        void UpdateCustomPipelines(RENDERER_CORE::DescriptorSetLayout& Layout,uint32_t FrameIndex);
        void CreatePostProcessingPassPipeline();
        std::array<std::unordered_map<uint64_t, size_t>, MAX_FRAMES_IN_FLIGHT> CustomPipelines;
	};
}
