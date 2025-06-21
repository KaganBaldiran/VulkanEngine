#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../vkcore/VulkanUtils.hpp" 
#include <vector>
#include <queue>

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

#include "../vkscene/Scene.hpp"
#include "../vkscene/Mesh.hpp"
#include "../vkscene/Camera.hpp"

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
        void RenderFrame(VKSCENE::Scene &Scene,VKSCENE::Camera3D &Camera);
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

        VKCORE::DescriptorSetLayout GbufferPassLayout;;
        std::vector<VkDescriptorSet> GbufferPassDescriptorSets;
        std::vector<VkDescriptorSetLayout> GbufferPassLayouts;

        VKCORE::TextureData DepthImage{};
        VkFormat DepthImageFormat;

        VKCORE::GraphicsPipeline GbufferGraphicsPipeline;
        VKCORE::GraphicsPipeline LightingGraphicsPipeline;
        VKCORE::GraphicsPipeline PhysicsDebugGraphicsPipeline;

        std::vector<VKCORE::FrameSyncObjects> SyncObjects;
        uint32_t CurrentFrame = 0;

        VKCORE::Buffer QuadVertexBuffer{};
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

