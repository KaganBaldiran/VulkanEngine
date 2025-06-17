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


#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

const int MAX_FRAMES_IN_FLIGHT = 3;

#ifdef NDEBUG
const bool EnableValidationLayers = false;
#else 
const bool EnableValidationLayers = true;
#endif // !NDEBUG

namespace VKAPP
{ 
    struct Matrixes;
    struct GeometryBuffer;

    static float QuadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };

    class Renderer
    {
    public:
        void Init(bool EnableValidationLayers);
        void RenderFrame(VKSCENE::Scene &Scene,VKSCENE::Camera3D &Camera);
        void Destroy();

        void CreateSceneBuffers(VKSCENE::Scene& Scene);
        void AppendSceneBuffersDestroyList(VKSCENE::Scene& Scene);

        VKCORE::Window Window;
        VKCORE::Instance Instance;
        VKCORE::Surface Surface;
        VKCORE::DeviceContext DeviceContext;

        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        VKCORE::SwapChain SwapChain;
        VKCORE::QueueFamilyIndices QueueFamilyIndices;
        uint32_t GraphicsQueueIndex;

        VKCORE::CommandPool CommandPool;
        std::vector<VkCommandBuffer> CommandBuffers;

        std::vector<VKCORE::Buffer> UBO;
        std::vector<void*> UBOmapped;

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

        std::vector<VKCORE::FrameSyncObjects> SyncObjects;
        uint32_t CurrentFrame = 0;

        VKCORE::Buffer QuadVertexBuffer{};
    private:
        void InitializeCoreSystems(bool EnableValidationLayers);
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
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame
        );

        std::queue<std::function<void(VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)>> RenderTasks;
        std::vector<VKCORE::Buffer*> SceneBufferDestroyList;
    };

    struct GeometryBuffer
    {
        VKCORE::TextureData PositionAttachment;
        VKCORE::TextureData NormalAttachment;

        void Create(
            VkPhysicalDevice& PhysicalDevice,
            VkDevice& LogicalDevice,
            const uint32_t& Width,
            const uint32_t& Height
        )
        {
            VKCORE::CreateImage(
                PhysicalDevice,
                LogicalDevice,
                Width,
                Height,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                PositionAttachment.Image,
                PositionAttachment.ImageMemory
            );
            PositionAttachment.ImageView = VKCORE::CreateImageView(PositionAttachment.Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
            VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, PositionAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

            VKCORE::CreateImage(
                PhysicalDevice,
                LogicalDevice,
                Width,
                Height,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                NormalAttachment.Image,
                NormalAttachment.ImageMemory
            );
            NormalAttachment.ImageView = VKCORE::CreateImageView(NormalAttachment.Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, LogicalDevice);
            VKCORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, NormalAttachment.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        }

        void Destroy(VkDevice& LogicalDevice)
        {
            PositionAttachment.Destroy(LogicalDevice);
            NormalAttachment.Destroy(LogicalDevice);
        }
    };

    struct Matrixes {
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
    };
}

