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

namespace VKAPP
{ 
    struct Matrixes;
    struct PersistentBuffer;

    class Renderer
    {
    public:
        void Initialize(RendererContext& DestinationRendererContext,bool EnablePhysicsDebugDrawing);
        void RenderFrame(VKSCENE::Scene &Scene);
        void Destroy();

        bool EnablePhysicsDebugDrawing;
        RendererContext* rendererContext = nullptr;
      
        //References
        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        uint32_t GraphicsQueueIndex;

        std::vector<VkCommandBuffer> CommandBuffers;

        std::vector<VKCORE::Buffer> UBO;
        std::vector<void*> UBOmapped;

        std::vector<VKCORE::PersistentBuffer> LightingPassUBOs;

        std::vector<VKCORE::PersistentBuffer> PhysicsDebugLineVertexBuffers;
        int MaxLines;
        VkDeviceSize PhysicsDebugLineVertexBuffersize;

        std::vector<GeometryBuffer> Gbuffers;

        VKCORE::DescriptorPool descriptorPool;
        VKCORE::DescriptorSetLayout LightingPassLayout;
        std::vector<VkDescriptorSet> LightingPassDescriptorSets;
        std::vector<VkDescriptorSetLayout> LightingPassLayouts;

        std::vector<VkDescriptorSet> GbufferPassDescriptorSets;
        std::vector<VkDescriptorSetLayout> GbufferPassLayouts;

        VKCORE::TextureData DepthImage{};

        VKCORE::GraphicsPipeline LightingPassGraphicsPipeline;
        VKCORE::GraphicsPipeline PhysicsDebugGraphicsPipeline;

        VKCORE::PipelineBarrier2 PipelineBarrier2;

        std::vector<VKCORE::FrameSyncObjects> SyncObjects;
        uint32_t CurrentFrame = 0;
    private:
        void InitializePipelines();
        void OnRecreateSwapChain();

        void RenderGeometryPass(
            VKSCENE::Scene& Scene,
            VKSCENE::Camera3D& Camera, 
            VkCommandBuffer& CommandBuffer, 
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        void RenderLightingPass(
            VKSCENE::Scene& Scene,
            VKSCENE::Camera3D &Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        void RenderPhysicsDebugPass(
            VKSCENE::Scene& Scene,
            VKSCENE::Camera3D& Camera,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        std::queue<std::function<void(VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)>> RenderTasks;
        std::vector<VKCORE::Buffer*> SceneBufferDestroyList;
    };

    struct Matrixes {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
    };
}

