#include "Renderer.hpp"
#include <array>
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


void VKAPP::Renderer::Init(bool EnableValidationLayers)
{
    InitializeCoreSystems(EnableValidationLayers);

    UBO.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(PhysicalDevice, LogicalDevice, sizeof(Matrixes), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, UBO[i]);
        void* DataPtr;
        if (vkMapMemory(LogicalDevice, UBO[i].BufferMemory, 0, sizeof(Matrixes), 0, &DataPtr) != VK_SUCCESS)
        {
            throw std::runtime_error("Unable to map memory!");
        }
        UBOmapped.push_back(DataPtr);
    }

    Gbuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Create(PhysicalDevice, LogicalDevice, SwapChain.Extent.width, SwapChain.Extent.height);
    }

    //Lighting pass descriptor set
    descriptorPool.Create(
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2 * MAX_FRAMES_IN_FLIGHT}},
        2 * MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.CreateLayout(LogicalDevice);

    LightingPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    LightingPassLayouts.resize(MAX_FRAMES_IN_FLIGHT, LightingPassLayout.descriptorSetLayout);
    VKCORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, descriptorPool.descriptorPool, LightingPassLayouts, LightingPassDescriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::WriteDescriptorSets(LogicalDevice, {}, { NormalTextureWrite,PositionTextureWrite});
    }


    //Geometry buffer pass descriptor set
    GbufferPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, VK_SHADER_STAGE_VERTEX_BIT);
    GbufferPassLayout.CreateLayout(LogicalDevice);

    GbufferPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    GbufferPassLayouts.resize(MAX_FRAMES_IN_FLIGHT, GbufferPassLayout.descriptorSetLayout);
    VKCORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, descriptorPool.descriptorPool, GbufferPassLayouts, GbufferPassDescriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteBuffer UBOwrite(UBO[i], sizeof(Matrixes), 0, GbufferPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VKCORE::WriteDescriptorSets(LogicalDevice, { UBOwrite }, {});
    }



    DepthImageFormat = VKCORE::FindSupportedFormat(PhysicalDevice, { VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, SwapChain.Extent.width, SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = VKCORE::CreateImageView(DepthImage.Image, DepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

    InitializePipelines();

    VKCORE::FrameSyncObjects SyncObject;
    SyncObject.FenceCreateFlag = VK_FENCE_CREATE_SIGNALED_BIT;
    SyncObjects.resize(MAX_FRAMES_IN_FLIGHT, SyncObject);
    VKCORE::AllocateFrameSyncObjects(LogicalDevice, SyncObjects);

    VKCORE::UploadDataToDeviceLocalBuffer(
        LogicalDevice,
        PhysicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        QuadVertices,
        sizeof(QuadVertices),
        QuadVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
}

void VKAPP::Renderer::RenderFrame(VKSCENE::Scene& Scene,VKSCENE::Camera3D& Camera)
{
    auto RenderTask = [&](VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame) {
        VkCommandBufferBeginInfo CommandBufferBeginInfo{};
        CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CommandBufferBeginInfo.flags = 0;
        CommandBufferBeginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer");
        }

        VKCORE::TransitionImageLayout(CommandBuffer, Gbuffers[CurrentFrame].NormalAttachment.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VKCORE::TransitionImageLayout(CommandBuffer, Gbuffers[CurrentFrame].PositionAttachment.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VKCORE::TransitionImageLayout(CommandBuffer, DepthImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        RenderGeometryPass(Scene, Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);

        VKCORE::TransitionImageLayout(CommandBuffer, Gbuffers[CurrentFrame].NormalAttachment.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VKCORE::TransitionImageLayout(CommandBuffer, Gbuffers[CurrentFrame].PositionAttachment.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VKCORE::TransitionImageLayout(CommandBuffer, SwapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        RenderLightingPass(CommandBuffer, CurrentImageIndex, CurrentFrame);

        VKCORE::TransitionImageLayout(CommandBuffer, SwapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer");
        }
    };

    auto onRecreateSwapChain = [&]()
    {
       OnRecreateSwapChain();
    };

    VKCORE::RenderFrame(
        LogicalDevice,
        DeviceContext.GraphicsQueue,
        DeviceContext.PresentQueue,
        CommandBuffers,
        { {0,RenderTask} },
        onRecreateSwapChain,
        SyncObjects,
        SwapChain,
        Window,
        CurrentFrame,
        MAX_FRAMES_IN_FLIGHT
    );
    glfwPollEvents();
}

void VKAPP::Renderer::RenderGeometryPass(
    VKSCENE::Scene &Scene,
    VKSCENE::Camera3D &Camera,
    VkCommandBuffer& CommandBuffer, 
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame
)
{
    Matrixes MatrixUBO;
    MatrixUBO.ViewMatrix = Camera.ViewMatrix;
    MatrixUBO.ProjectionMatrix = Camera.ProjectionMatrix;
    memcpy(UBOmapped[CurrentFrame], &MatrixUBO, sizeof(MatrixUBO));

    std::array<VkClearValue, 3> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[2].depthStencil = { 1.0f,0 };

    VKCORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        Gbuffers[CurrentFrame].NormalAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );
    RenderingPass.AppendAttachment(
        Gbuffers[CurrentFrame].PositionAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );
    RenderingPass.AppendAttachment(
        DepthImage.ImageView,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[2]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)SwapChain.Extent.width, (uint32_t)SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GbufferGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { Scene.SceneVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    vkCmdBindIndexBuffer(CommandBuffer, Scene.SceneIndexBuffer.BufferObject, 0, VK_INDEX_TYPE_UINT32);

    VkViewport Viewport{};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = static_cast<float>(SwapChain.Extent.width);
    Viewport.height = static_cast<float>(SwapChain.Extent.height);
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor{};
    Scissor.offset = { 0,0 };
    Scissor.extent = SwapChain.Extent;
    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GbufferGraphicsPipeline.Layout,0,1,&GbufferPassDescriptorSets[CurrentFrame],0,nullptr);

    for (auto& Entity : Scene.Entities)
    {
        auto& Model = Entity->Model;
        glm::mat4 ModelMatrix = Model.transformation.GetModelMatrix();
        vkCmdPushConstants(
            CommandBuffer,
            GbufferGraphicsPipeline.Layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &ModelMatrix
        );
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
}
void VKAPP::Renderer::RenderLightingPass(VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)
{
    std::array<VkClearValue, 2> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].depthStencil = { 1.0f,0 };

    VKCORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)SwapChain.Extent.width, (uint32_t)SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);

    VkViewport Viewport{};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = static_cast<float>(SwapChain.Extent.width);
    Viewport.height = static_cast<float>(SwapChain.Extent.height);
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor{};
    Scissor.offset = { 0,0 };
    Scissor.extent = SwapChain.Extent;
    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGraphicsPipeline.Layout, 0, 1, &LightingPassDescriptorSets[CurrentFrame], 0, nullptr);

    vkCmdDraw(CommandBuffer,4,1,0,0);

    RenderingPass.EndRendering(CommandBuffer);
};

void VKAPP::Renderer::CreateSceneBuffers(VKSCENE::Scene& Scene)
{
    std::vector<VKSCENE::Model3D*> Models(Scene.Entities.size());
    for (size_t i = 0; i < Scene.Entities.size(); i++)
    {
        Models[i] = &Scene.Entities[i]->Model;
    }

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
        Scene.SceneVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    VKCORE::UploadDataToDeviceLocalBuffer(
        LogicalDevice,
        PhysicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        ModelBatch.Indices.data(),
        IndexBufferSize,
        Scene.SceneIndexBuffer,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
}

void VKAPP::Renderer::AppendSceneBuffersDestroyList(VKSCENE::Scene& Scene)
{
    SceneBufferDestroyList.push_back(&Scene.SceneVertexBuffer);
    SceneBufferDestroyList.push_back(&Scene.SceneIndexBuffer);
}

void VKAPP::Renderer::InitializeCoreSystems(bool EnableValidationLayers)
{
    if (!glfwInit())
    {
        throw std::runtime_error("Unable to initialize GLFW");
    }

    VKCORE::VulkanWindowCreateInfo WindowCreateInfo{};
    WindowCreateInfo.WindowInitialHeight = 800;
    WindowCreateInfo.WindowInitialWidth = 1000;
    WindowCreateInfo.WindowsName = "Hello World";
    Window.Create(WindowCreateInfo);

    VKCORE::VulkanInstanceCreateInfo InstanceCreateInfo{};
    InstanceCreateInfo.APIVersion = VK_API_VERSION_1_3;
    InstanceCreateInfo.ApplicationName = "Application";
    InstanceCreateInfo.ApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineName = "No Engine";
    InstanceCreateInfo.EnableValidationLayers = EnableValidationLayers;
    InstanceCreateInfo.ValidationLayersToEnable = { "VK_LAYER_KHRONOS_validation" };
    Instance.Create(InstanceCreateInfo);

    Surface.Create(Instance.instance, Window.window);

    VKCORE::VulkanDeviceCreateInfo DeviceCreateInfo{};
    DeviceCreateInfo.DeviceExtensionsToEnable = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
    DeviceCreateInfo.QueuePriority = 1.0f;
    DeviceContext.Create(DeviceCreateInfo, Surface.surface, Instance.instance);

    LogicalDevice = DeviceContext.logicalDevice;
    PhysicalDevice = DeviceContext.physicalDevice;
    SwapChain.Create(PhysicalDevice, LogicalDevice, Surface.surface, Window.window);

    QueueFamilyIndices = VKCORE::FindQueueFamilies(PhysicalDevice, Surface.surface);
    GraphicsQueueIndex = QueueFamilyIndices.GraphicsFamily.value();
    CommandPool.Create(GraphicsQueueIndex, LogicalDevice);

    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateCommandBuffers(CommandPool.commandPool, LogicalDevice, CommandBuffers);
}

void VKAPP::Renderer::InitializePipelines()
{
    VKCORE::ShaderModule LightingVertexShaderModule("shaders\\LightingPassShader.vert", "shaders\\LightingPassShaderVert.spv", true, LogicalDevice);
    VKCORE::ShaderModule LightingFragmentShaderModule("shaders\\LightingPassShader.frag", "shaders\\LightingPassShaderFrag.spv", true, LogicalDevice);

    VKCORE::ShaderModule GbufferVertexShaderModule("shaders\\GeometryBufferShader.vert", "shaders\\GeometryBufferShaderVert.spv", true, LogicalDevice);
    VKCORE::ShaderModule GbufferFragmentShaderModule("shaders\\GeometryBufferShader.frag", "shaders\\GeometryBufferShaderFrag.spv", true, LogicalDevice);

    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    PushConstantRange.size = sizeof(glm::mat4);
    PushConstantRange.offset = 0;

    VKCORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {&GbufferVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&GbufferFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 2;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R32G32B32A32_SFLOAT , VK_FORMAT_R32G32B32A32_SFLOAT };
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = DepthImageFormat;
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.AttributeDescriptions = VKSCENE::Vertex3D::GetAttributeDescriptions();
    PipelineCreateInfo.BindingDescription = VKSCENE::Vertex3D::GetBindingDescription();
    PipelineCreateInfo.DescriptorSetLayouts = GbufferPassLayouts;
    PipelineCreateInfo.PushConstantRanges = { PushConstantRange };
    GbufferGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

    VKCORE::VertexInputDescription QuadVertexDescription{};
    QuadVertexDescription.SetBindingDescription(0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX);
    QuadVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    QuadVertexDescription.AppendAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3);

    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R8G8B8A8_SRGB };
    PipelineCreateInfo.ShaderModules = { {&LightingVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&LightingFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.AttributeDescriptions = QuadVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = QuadVertexDescription.BindingDescription;
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineCreateInfo.DescriptorSetLayouts = LightingPassLayouts;
    PipelineCreateInfo.PushConstantRanges = {};
    LightingGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

    LightingVertexShaderModule.Destroy(LogicalDevice);
    LightingFragmentShaderModule.Destroy(LogicalDevice);
    GbufferVertexShaderModule.Destroy(LogicalDevice);
    GbufferFragmentShaderModule.Destroy(LogicalDevice);
}

void VKAPP::Renderer::OnRecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(Window.window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(Window.window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(LogicalDevice);

    SwapChain.Destroy(LogicalDevice);
    DepthImage.Destroy(LogicalDevice);

    SwapChain.Create(PhysicalDevice, LogicalDevice, Surface.surface, Window.window);

    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, SwapChain.Extent.width, SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = VKCORE::CreateImageView(DepthImage.Image, DepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
        Gbuffer.Create(PhysicalDevice, LogicalDevice, SwapChain.Extent.width, SwapChain.Extent.height);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::WriteDescriptorSets(LogicalDevice, {}, { NormalTextureWrite,PositionTextureWrite });
    }
}

void VKAPP::Renderer::Destroy()
{
    vkDeviceWaitIdle(LogicalDevice);
    VKCORE::DestroyFrameSyncObjects(LogicalDevice, SyncObjects);
    DepthImage.Destroy(LogicalDevice);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UBO[i].Destroy(LogicalDevice);
    }
    UBO.clear();
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
    }
    Gbuffers.clear();
    for (auto& SceneBuffer : SceneBufferDestroyList)
    {
        SceneBuffer->Destroy(LogicalDevice);
    }
    SceneBufferDestroyList.clear();
    LightingPassLayout.Destroy(LogicalDevice);
    GbufferPassLayout.Destroy(LogicalDevice);
    descriptorPool.Destroy(LogicalDevice);
    CommandPool.Destroy(LogicalDevice);
    QuadVertexBuffer.Destroy(LogicalDevice);
    LightingGraphicsPipeline.Destroy(LogicalDevice);
    GbufferGraphicsPipeline.Destroy(LogicalDevice);
    Surface.Destroy(Instance.instance);
    SwapChain.Destroy(LogicalDevice);
    DeviceContext.Destroy();
    Instance.Destroy();
    Window.Destroy();
    glfwTerminate();
};
