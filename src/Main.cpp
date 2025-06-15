#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <fstream>
#include <array>
#include <queue>
#include <future>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "VKCORE/VulkanCommandPool.hpp"
#include "VKCORE/VulkanCommandBuffer.hpp"
#include "VKCORE/VulkanDescriptorPool.hpp"
#include "VKCORE/VulkanDescriptorSetLayout.hpp"
#include "VKCORE/VulkanDescriptorSet.hpp"
#include "VKCORE/VulkanDevice.hpp"
#include "VKCORE/VulkanPipeline.hpp"
#include "VKCORE/VulkanImage.hpp"
#include "VKCORE/VulkanImageView.hpp"
#include "VKCORE/VulkanWindow.hpp"
#include "VKCORE/VulkanDevice.hpp"
#include "VKCORE/VulkanSwapChain.hpp"
#include "VKCORE/VulkanInstance.hpp"
#include "VKCORE/VulkanRender.hpp"
#include "VKCORE/VulkanBuffer.hpp"

#include "VKSCENE/Mesh.hpp"
#include "VKSCENE/Camera.hpp"

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

const int MAX_FRAMES_IN_FLIGHT = 3;

#ifdef NDEBUG
const bool EnableValidationLayers = false;
#else 
const bool EnableValidationLayers = true;
#endif // !NDEBUG

class PhysicsContext
{
public:
    std::shared_ptr<btDefaultCollisionConfiguration> CollisionConfiguration;
    std::shared_ptr<btCollisionDispatcher> Dispatcher;
    std::shared_ptr<btBroadphaseInterface> OverlappingPairCache;
    std::shared_ptr<btSequentialImpulseConstraintSolver> Solver;
    std::shared_ptr<btDiscreteDynamicsWorld> DynamicsWorld;

    PhysicsContext()
    {
        CollisionConfiguration = std::make_shared<btDefaultCollisionConfiguration>();
        Dispatcher = std::make_shared < btCollisionDispatcher>(CollisionConfiguration.get());
        OverlappingPairCache = std::make_shared<btDbvtBroadphase>();
        Solver = std::make_shared<btSequentialImpulseConstraintSolver>();
        DynamicsWorld = std::make_shared<btDiscreteDynamicsWorld>(Dispatcher.get(), OverlappingPairCache.get(), 
            Solver.get(), CollisionConfiguration.get());
    };
};

struct Matrixes {
    glm::mat4 ModelMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
};

void UpdateUBO(VKSCENE::Camera3D &Camera,VKCORE::Window &window,void* DestinationBufferMapped,const VkExtent2D &Extent)
{
    Camera.Update(window);
    Camera.UpdateMatrix({ Extent.width,Extent.height });

    Matrixes MatrixUBO;
    MatrixUBO.ModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
    MatrixUBO.ViewMatrix = Camera.ViewMatrix;
    MatrixUBO.ProjectionMatrix = Camera.ProjectionMatrix;
    memcpy(DestinationBufferMapped, &MatrixUBO, sizeof(MatrixUBO));
}

int main()
{
    try
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Unable to initialize GLFW");
            return -1;
        }

        VKCORE::VulkanWindowCreateInfo WindowCreateInfo{};
        WindowCreateInfo.WindowInitialHeight = 800;
        WindowCreateInfo.WindowInitialWidth = 1000;
        WindowCreateInfo.WindowsName = "Hello World";
        VKCORE::Window Window(WindowCreateInfo);

        VKCORE::VulkanInstanceCreateInfo InstanceCreateInfo{};
        InstanceCreateInfo.APIVersion = VK_API_VERSION_1_3;
        InstanceCreateInfo.ApplicationName = "Application";
        InstanceCreateInfo.ApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        InstanceCreateInfo.EngineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        InstanceCreateInfo.EngineName = "No Engine";
        InstanceCreateInfo.EnableValidationLayers = EnableValidationLayers;
        InstanceCreateInfo.ValidationLayersToEnable = { "VK_LAYER_KHRONOS_validation" };
        VKCORE::Instance Instance(InstanceCreateInfo);

        VKCORE::Surface Surface(Instance.instance, Window.window);

        VKCORE::VulkanDeviceCreateInfo DeviceCreateInfo{};
        DeviceCreateInfo.DeviceExtensionsToEnable = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
        DeviceCreateInfo.QueuePriority = 1.0f;
        VKCORE::DeviceContext DeviceContext(DeviceCreateInfo,Surface.surface,Instance.instance);

        auto& LogicalDevice = DeviceContext.logicalDevice;
        auto& PhysicalDevice = DeviceContext.physicalDevice;
        VKCORE::SwapChain swapChain(PhysicalDevice,LogicalDevice,Surface.surface,Window.window);

        auto GraphicsQueueIndex = VKCORE::FindQueueFamilies(PhysicalDevice, Surface.surface).GraphicsFamily.value();
        VKCORE::CommandPool CommandPool(GraphicsQueueIndex, LogicalDevice);

        std::vector<VkCommandBuffer> CommandBuffers(MAX_FRAMES_IN_FLIGHT);
        VKCORE::AllocateCommandBuffers(CommandPool.commandPool, LogicalDevice, CommandBuffers);

        VKCORE::TextureData Texture0;
        VKCORE::CreateTextureImage("resources\\image.png", PhysicalDevice, LogicalDevice, CommandPool.commandPool,
            DeviceContext.GraphicsQueue, Texture0);

        std::vector<VKCORE::Buffer> UBO(MAX_FRAMES_IN_FLIGHT);
        std::vector<void*> UBOmapped;
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VKCORE::CreateBuffer(PhysicalDevice, LogicalDevice, sizeof(Matrixes), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UBO[i]);
            void* DataPtr;
            vkMapMemory(LogicalDevice, UBO[i].BufferMemory, 0, sizeof(Matrixes), 0, &DataPtr);
            UBOmapped.push_back(DataPtr);
        }

        double StartingTime = glfwGetTime();
        std::vector<std::thread> ImportModelThreads;
        std::vector<std::future<void>> Futures;
        std::vector<std::string> FilePaths;

        FilePaths.push_back("resources\\sponza.obj");
        FilePaths.push_back("resources\\shovel2.obj");

        std::vector<VKSCENE::Model3D> Models(FilePaths.size());
        for (size_t i = 0; i < FilePaths.size(); i++)
        {
            Futures.push_back(std::async(std::launch::async,VKSCENE::Import3Dmodel, FilePaths[i].c_str(),std::ref(Models[i])));
        }
        for (auto& future : Futures)
        {
            future.get();
        }

        double DeltaTime = glfwGetTime() - StartingTime;
        std::cout << "Models were imported in: " << DeltaTime << " seconds" << std::endl;

        VKCORE::Buffer VertexBuffer{};
        VKCORE::Buffer IndexBuffer{};
        {
            VKSCENE::BatchInfo ModelBatch = VKSCENE::BatchModels(Models);
            VkDeviceSize VertexBufferSize = ModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
            VkDeviceSize IndexBufferSize = ModelBatch.Indices.size() * sizeof(uint32_t);

            VKCORE::UploadDataToDeviceLocalBuffer(
                LogicalDevice,
                PhysicalDevice,
                CommandPool.commandPool,
                DeviceContext.GraphicsQueue,
                ModelBatch.Vertices.data(),
                VertexBufferSize,
                VertexBuffer,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            );

            VKCORE::UploadDataToDeviceLocalBuffer(
                LogicalDevice,
                PhysicalDevice,
                CommandPool.commandPool,
                DeviceContext.GraphicsQueue,
                ModelBatch.Indices.data(),
                IndexBufferSize,
                IndexBuffer,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            );
        }

        VKCORE::DescriptorPool descriptorPool({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MAX_FRAMES_IN_FLIGHT}
            }, MAX_FRAMES_IN_FLIGHT, LogicalDevice);

        VKCORE::DescriptorSetLayout Layout;
        Layout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, VK_SHADER_STAGE_VERTEX_BIT);
        Layout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
        Layout.CreateLayout(LogicalDevice);

        std::vector<VkDescriptorSet> DescriptorSets(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> Layouts(MAX_FRAMES_IN_FLIGHT, Layout.descriptorSetLayout);
        VKCORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, descriptorPool.descriptorPool, Layouts, DescriptorSets);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VKCORE::DescriptorSetWriteBuffer UBOwrite(UBO[i], sizeof(Matrixes), 0, DescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            VKCORE::DescriptorSetWriteImage TextureWrite(Texture0.ImageView, Texture0.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            VKCORE::WriteDescriptorSets(LogicalDevice, { UBOwrite }, { TextureWrite });
        }

        VKCORE::TextureData DepthImage{};
        VkFormat DepthImageFormat = VKCORE::FindSupportedFormat(PhysicalDevice,{ VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        VKCORE::CreateImage(PhysicalDevice,LogicalDevice, swapChain.Extent.width, swapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
        DepthImage.ImageView = VKCORE::CreateImageView(DepthImage.Image, DepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT,LogicalDevice);

        VKCORE::ShaderModule VertexShaderModule("shaders\\09_shader_base.vert","shaders\\vert2.spv", EnableValidationLayers,LogicalDevice);
        VKCORE::ShaderModule FragmentShaderModule("shaders\\09_shader_base.frag","shaders\\frag2.spv", EnableValidationLayers, LogicalDevice);

        VKCORE::ShaderModuleInfo VertexShaderModuleInfo;
        VertexShaderModuleInfo.Usage = VK_SHADER_STAGE_VERTEX_BIT;
        VertexShaderModuleInfo.Module = &VertexShaderModule;

        VKCORE::ShaderModuleInfo FragmentShaderModuleInfo;
        FragmentShaderModuleInfo.Usage = VK_SHADER_STAGE_FRAGMENT_BIT;
        FragmentShaderModuleInfo.Module = &FragmentShaderModule;

        VKCORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
        PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
        PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PipelineCreateInfo.ViewportWidth = static_cast<float>(swapChain.Extent.width);
        PipelineCreateInfo.ViewportHeight = static_cast<float>(swapChain.Extent.height);
        PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
        PipelineCreateInfo.ShaderModules = { VertexShaderModuleInfo ,FragmentShaderModuleInfo };
        PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
        PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R8G8B8A8_SRGB };
        PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = DepthImageFormat;
        PipelineCreateInfo.RenderPass = nullptr;
        PipelineCreateInfo.ScissorOffset = { 0,0 };
        PipelineCreateInfo.ScissorExtent = { swapChain.Extent.width ,swapChain.Extent.height };
        PipelineCreateInfo.ViewportMinDepth = 0.0f;
        PipelineCreateInfo.ViewportMaxDepth = 1.0f;
        PipelineCreateInfo.AttributeDescriptions = VKSCENE::Vertex3D::GetAttributeDescriptions();
        PipelineCreateInfo.BindingDescription = VKSCENE::Vertex3D::GetBindingDescription();
        PipelineCreateInfo.DescriptorSetLayouts = Layouts;
        VKCORE::GraphicsPipeline GraphicsPipeline(PipelineCreateInfo, LogicalDevice);

        VKCORE::FrameSyncObjects SyncObject;
        SyncObject.FenceCreateFlag = VK_FENCE_CREATE_SIGNALED_BIT;
        std::vector<VKCORE::FrameSyncObjects> SyncObjects(MAX_FRAMES_IN_FLIGHT, SyncObject);
        
        VKCORE::AllocateFrameSyncObjects(LogicalDevice, SyncObjects);

        VKSCENE::Camera3D Camera(Window);

        PhysicsContext PhyContext;
        PhyContext.DynamicsWorld->setGravity({ 0,-10,0 });
        btAlignedObjectArray<btCollisionShape*> CollisionShapes;

        std::unique_ptr<btTriangleMesh> TriangleMesh = std::make_unique<btTriangleMesh>();        
        for (auto &Mesh : Models[0].Meshes)
        {
            for (size_t y = 0; y < Mesh.Indices.size(); y += 3)
            {
                auto &Triangle0 = Mesh.Vertices[Mesh.Indices[y]].Position;
                auto &Triangle1 = Mesh.Vertices[Mesh.Indices[y + 1]].Position;
                auto &Triangle2 = Mesh.Vertices[Mesh.Indices[y + 2]].Position;
                TriangleMesh->addTriangle(
                    { Triangle0.x,Triangle0.y ,Triangle0.z }, 
                    { Triangle1.x,Triangle1.y ,Triangle1.z },
                    { Triangle2.x,Triangle2.y ,Triangle2.z }
                );
            }
        }
        std::unique_ptr<btBvhTriangleMeshShape> StaticMeshShape = std::make_unique<btBvhTriangleMeshShape>(TriangleMesh.get(),true);
        StaticMeshShape->setLocalScaling(btVector3(0.1f,0.1f,0.1f));
        CollisionShapes.push_back(StaticMeshShape.get());

        btTransform GroundTransform;
        GroundTransform.setIdentity();
        GroundTransform.setOrigin({ 0,0,0 });

        std::unique_ptr<btDefaultMotionState> GroundMotionState = std::make_unique<btDefaultMotionState>(GroundTransform);
        btRigidBody::btRigidBodyConstructionInfo GroundRigidBodyCreateInfo(0, GroundMotionState.get(), StaticMeshShape.get());
        std::unique_ptr<btRigidBody> GroundRigidBody = std::make_unique<btRigidBody>(GroundRigidBodyCreateInfo);

        PhyContext.DynamicsWorld->addRigidBody(GroundRigidBody.get());

        auto OnRecreateSwapChain = [&]() {
            int width = 0, height = 0;
            glfwGetFramebufferSize(Window.window, &width, &height);
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(Window.window, &width, &height);
                glfwWaitEvents();
            }
            vkDeviceWaitIdle(LogicalDevice);

            swapChain.Destroy(LogicalDevice);
            DepthImage.Destroy(LogicalDevice);

            swapChain.Create(PhysicalDevice, LogicalDevice, Surface.surface, Window.window);
            
            VKCORE::CreateImage(PhysicalDevice, LogicalDevice, swapChain.Extent.width, swapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
            DepthImage.ImageView = VKCORE::CreateImageView(DepthImage.Image, DepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);
        };

        auto RecordCommandBuffer = [&](VkCommandBuffer &CommandBuffer,uint32_t CurrentImageIndex,uint32_t CurrentFrame) {
            UpdateUBO(Camera,Window, UBOmapped[CurrentFrame], swapChain.Extent);

            std::array<VkClearValue, 2> ClearColors{};
            ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
            ClearColors[1].depthStencil = { 1.0f,0 };

            VKCORE::DynamicRenderingPass RenderingPass;
            RenderingPass.AppendAttachment(
                swapChain.SwapChainImagesViews[CurrentImageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                VK_ATTACHMENT_LOAD_OP_CLEAR, 
                VK_ATTACHMENT_STORE_OP_STORE, 
                ClearColors[0]
            );

            RenderingPass.AppendAttachment(
                DepthImage.ImageView,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                ClearColors[1]
            );

            VkCommandBufferBeginInfo CommandBufferBeginInfo{};
            CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            CommandBufferBeginInfo.flags = 0;
            CommandBufferBeginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer");
            }

            VKCORE::TransitionImageLayout(CommandBuffer, swapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            VKCORE::TransitionImageLayout(CommandBuffer, DepthImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

            RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)swapChain.Extent.width, (uint32_t)swapChain.Extent.height} });

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline.pipeline);

            VkBuffer VertexBuffers[] = { VertexBuffer.BufferObject };
            VkDeviceSize Offsets[] = { 0 };
            vkCmdBindVertexBuffers(CommandBuffer,0,1,VertexBuffers,Offsets);
            vkCmdBindIndexBuffer(CommandBuffer,IndexBuffer.BufferObject,0, VK_INDEX_TYPE_UINT32);

            VkViewport Viewport{};
            Viewport.x = 0.0f;
            Viewport.y = 0.0f;
            Viewport.width = static_cast<float>(swapChain.Extent.width);
            Viewport.height = static_cast<float>(swapChain.Extent.height);
            Viewport.minDepth = 0.0f;
            Viewport.maxDepth = 1.0f;
            vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

            VkRect2D Scissor{};
            Scissor.offset = { 0,0 };
            Scissor.extent = swapChain.Extent;
            vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
            vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline.Layout, 0, 1, &DescriptorSets[CurrentFrame], 0, nullptr);
           
            for (auto &Model : Models)
            {
                for (auto& Mesh : Model.Meshes)
                {
                    if (!Mesh.Enabled) continue;
                    vkCmdDrawIndexed(CommandBuffer, 
                        static_cast<uint32_t>(Mesh.Info.IndexCount),
                        1, 
                        static_cast<uint32_t>(Mesh.Info.FirstIndex),
                        static_cast<uint32_t>(Mesh.Info.VertexOffset),
                        0
                    );
                }
            }
            
            RenderingPass.EndRendering(CommandBuffer);

            VKCORE::TransitionImageLayout(CommandBuffer, swapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer");
            }
        };

        uint32_t CurrentFrame = 0;
        while (!glfwWindowShouldClose(Window.window))
        {
            glm::vec3 AllowMove = { 1,1,1 };
            btVector3 From = { Camera.CameraPosition.x,Camera.CameraPosition.y ,Camera.CameraPosition.z };
            btVector3 To = From + btVector3{ Camera.CameraDirection.x, Camera.CameraDirection.y, Camera.CameraDirection.z };

            btCollisionWorld::ClosestRayResultCallback RayCallBack0(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack0);
            if (RayCallBack0.hasHit()) AllowMove.x = 0;

            To = From + btVector3{ -Camera.CameraRight.x, -Camera.CameraRight.y, -Camera.CameraRight.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack1(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack1);
            if (RayCallBack1.hasHit()) AllowMove.y = 0;
           
            To = From + btVector3{ Camera.CameraRight.x, Camera.CameraRight.y, Camera.CameraRight.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack2(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack2);
            if (RayCallBack2.hasHit()) AllowMove.z = 0;
          
            Camera.AllowMove = AllowMove;

            VKCORE::RenderFrame(
                LogicalDevice,
                DeviceContext.GraphicsQueue,
                DeviceContext.PresentQueue,
                CommandBuffers,
                { {0,RecordCommandBuffer} },
                OnRecreateSwapChain,
                SyncObjects,
                swapChain,
                Window,
                CurrentFrame,
                MAX_FRAMES_IN_FLIGHT
            );
            glfwPollEvents();  
        }

        vkDeviceWaitIdle(LogicalDevice);
        VKCORE::DestroyFrameSyncObjects(LogicalDevice, SyncObjects);
        DepthImage.Destroy(LogicalDevice);
        Texture0.Destroy(LogicalDevice);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            UBO[i].Destroy(LogicalDevice);
        }
        VertexBuffer.Destroy(LogicalDevice);
        IndexBuffer.Destroy(LogicalDevice);
        VertexShaderModule.Destroy(LogicalDevice);
        FragmentShaderModule.Destroy(LogicalDevice);
        Layout.Destroy(LogicalDevice);
        descriptorPool.Destroy(LogicalDevice);
        CommandPool.Destroy(LogicalDevice);
        GraphicsPipeline.Destroy(LogicalDevice);
        Surface.Destroy(Instance.instance);
        swapChain.Destroy(LogicalDevice);
        DeviceContext.Destroy();
        Instance.Destroy();
        Window.Destroy();
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}