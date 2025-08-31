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

#include "GeometryBuffer.hpp"
#include "RendererContext.hpp"

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

        void RenderFrame(SCENE::Scene &Scene);
        void Destroy() override;

        bool EnablePhysicsDebugDrawing;
        RendererContext* rendererContext = nullptr;
      
        //References
        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        uint32_t GraphicsQueueIndex;

        std::vector<VkCommandBuffer> CommandBuffers;

        //std::vector<RENDERER_CORE::Buffer> UBO;
        //std::vector<void*> UBOmapped;

        //std::vector<RENDERER_CORE::PersistentBuffer> LightingPassUBOs;

        std::vector<RENDERER_CORE::PersistentBuffer> PhysicsDebugLineVertexBuffers;
        int MaxLines;
        VkDeviceSize PhysicsDebugLineVertexBuffersize;

        std::vector<GeometryBuffer> Gbuffers;

        RENDERER_CORE::DescriptorPool descriptorPool;
        RENDERER_CORE::DescriptorSetLayout LightingPassLayout;
        std::vector<VkDescriptorSet> LightingPassDescriptorSets;
        std::vector<VkDescriptorSetLayout> LightingPassLayouts;

        RENDERER_CORE::TextureData DepthImage{};

        RENDERER_CORE::GraphicsPipeline LightingPassGraphicsPipeline;
        RENDERER_CORE::GraphicsPipeline PhysicsDebugGraphicsPipeline;

        RENDERER_CORE::PipelineBarrier2 PipelineBarrier2;

        std::vector<RENDERER_CORE::FrameSyncObjects> SyncObjects;
        uint32_t CurrentFrame = 0;
    private:
        void InitializePipelines();
        void OnRecreateSwapChain();

        void RenderGeometryPass(
            SCENE::Scene& Scene,
            SCENE::Camera3D& Camera, 
            VkCommandBuffer& CommandBuffer, 
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        void RenderLightingPass(
            SCENE::Scene& Scene,
            SCENE::Camera3D &Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        void RenderPhysicsDebugPass(
            SCENE::Scene& Scene,
            SCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        std::queue<std::function<void(VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)>> RenderTasks;
        std::vector<RENDERER_CORE::Buffer*> SceneBufferDestroyList;
    };

    struct Matrixes {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
    };
}

