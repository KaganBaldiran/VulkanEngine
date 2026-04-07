#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "Core/VulkanUtils.hpp" 
#include <vector>
#include <queue>

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
#include "Core/VulkanSynchoronization.hpp"

#include "../Scene/Scene.hpp"
#include "../Scene/Mesh.hpp"
#include "../Scene/Camera.hpp"
#include "../Scene/Cubemap.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/Hash.hpp"

#include "GeometryBuffer.hpp"
#include "RendererContext.hpp"
#include "RenderPipeline.hpp"
#include "DeferredRenderPipeline.hpp"
#include "RenderGraph.hpp"

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

#ifdef NDEBUG
const bool EnableValidationLayers = false;
#else 
const bool EnableValidationLayers = true;
#endif // !NDEBUG

namespace RENDERER
{ 
    //Forward declarations.
    struct Matrixes;
    struct PersistentBuffer;

    /// <summary>
    /// Main deferred renderer class.
    /// It renders the scene using the buffers passed along from the scene class.
    /// Rendering is done on the renderer context given whilst the renderer creation.
    /// Context cannot be changed past the creation phase.
    /// </summary>
    class Renderer : COMMON::Destructible
    {
    public:
        Renderer(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing);
        Renderer() = default;
        void Create(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing);

        void AddRenderPass(RenderPassConfiguration RenderPass);
        void RemoveRenderPass(std::string RenderPassName);

        void AddRenderTask(FrameGraphTask RenderTask);

        void RenderFrame();
        void Destroy() override;

        bool EnablePhysicsDebugDrawing;
        RendererContext* RendererContextPtr = nullptr;
      
        //References
        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        uint32_t GraphicsQueueIndex;

        std::vector<VkCommandBuffer> CommandBuffers;
        std::vector<RENDERER_CORE::Buffer> PhysicsDebugLineVertexBuffers;
        int MaxLines;
        VkDeviceSize PhysicsDebugLineVertexBuffersize;

        std::array<RENDERER_CORE::ImageData, MAX_FRAMES_IN_FLIGHT> DepthImages;
        std::array<RENDERER_CORE::ImageData, MAX_FRAMES_IN_FLIGHT> ColorRenderAttachmentImages;
        std::array<GeometryBuffer, MAX_FRAMES_IN_FLIGHT> Gbuffers;

        RENDERER_CORE::DescriptorPool DescriptorPool;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> LightingPassDescriptorSets;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> PostProcessingPassDescriptorSets;

        RENDERER_CORE::GraphicsPipeline PhysicsDebugGraphicsPipeline;
        RENDERER_CORE::PipelineBarrier2 PipelineBarrier2;

        RENDERER_CORE::TimelineSemaphore TimelineSemaphore;
        uint64_t GlobalTimelineCounter = 0;
        std::vector<RENDERER_CORE::FrameSyncObjects> SyncObjects;
        uint32_t CurrentFrame = 0;

        RENDERER_CORE::FrameManager FrameManager;
        RENDERER::FrameGraph FrameGraph;

        RENDERER_CORE::ComputePipeline TestPipeline;
        RENDERER_CORE::Buffer TestBuffer;
        RENDERER_CORE::Descriptor<1> TestDescriptor;
        bool ShouldTest = true;

        void CreateTestResources();
        void DestroyTestResources();
        void DispatchComputeTest(
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );  
    private:
        void InitializePipelines();
        void OnRecreateSwapChain();

        void RenderPhysicsDebugPass(
            SCENE::Scene& Scene,
            SCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );
        void RenderPostProcessPass(
            SCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );
        void DispatchComputeCulling(
            SCENE::Scene& Scene,
            SCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            RENDERER_CORE::PipelineBarrier2& PipelineBarrier2,
            bool EnableCulling = true
        );
        void DispatchComputeResetCulling(
            SCENE::Scene& Scene,
            SCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            RENDERER_CORE::PipelineBarrier2& PipelineBarrier2,
            bool EnableCulling = true
        );

        std::vector<RenderPassConfiguration> RenderPasses;
        COMMON::VectorMap<std::pair<uint64_t, uint64_t>, std::pair<SCENE::Scene*,SCENE::Camera3D*>,COMMON::PairHash<uint64_t>> UniqueSceneCameraPairs;
    };

    struct Matrixes {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
    };

    struct Viewport {
        float X;
        float Y;
        float Width;
        float Height;
        float MinDepth;
        float MaxDepth;
    };

    struct Scissor {
        VkOffset2D Offset;
        VkExtent2D Extent;
    };
}

