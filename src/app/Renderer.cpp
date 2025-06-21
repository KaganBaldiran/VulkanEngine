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

static float QuadVertices[] = {
    // positions        // texture Coords
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
};

struct LightingPassUBOdata
{
    glm::vec4 CameraDirection;
    glm::vec4 CameraPosition;
    int StaticLightCount;
    int DynamicLightCount;
};

void VKAPP::Renderer::Initialize(RendererContext& DestinationRendererContext,bool EnablePhysicsDebugDrawing)
{
    this->EnablePhysicsDebugDrawing = EnablePhysicsDebugDrawing;
    this->rendererContext = &DestinationRendererContext;

    LogicalDevice = DestinationRendererContext.DeviceContext.logicalDevice;
    PhysicalDevice = DestinationRendererContext.DeviceContext.physicalDevice;

    GraphicsQueueIndex = DestinationRendererContext.QueueFamilyIndices.GraphicsFamily.value();

    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateCommandBuffers(rendererContext->CommandPool.commandPool, LogicalDevice, CommandBuffers);

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
        Gbuffer.Create(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);
    }

    LightingPassUBOs.resize(MAX_FRAMES_IN_FLIGHT);
    const VkDeviceSize LightingPassUBOsize = sizeof(LightingPassUBOdata);
    for (auto& LightingPassUBO : LightingPassUBOs)
    {
        VKCORE::CreateBuffer(
            PhysicalDevice, 
            LogicalDevice,
            LightingPassUBOsize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            LightingPassUBO.Buffer
        );
        LightingPassUBO.Map(LogicalDevice, 0, LightingPassUBOsize, 0);
    }

    //Lighting pass descriptor set
    descriptorPool.Create(
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2 * MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2 * MAX_FRAMES_IN_FLIGHT}},
        2 * MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.CreateLayout(LogicalDevice);

    LightingPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    LightingPassLayouts.resize(MAX_FRAMES_IN_FLIGHT, LightingPassLayout.descriptorSetLayout);
    VKCORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, descriptorPool.descriptorPool, LightingPassLayouts, LightingPassDescriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::DescriptorSetWriteBuffer UBOwrite(LightingPassUBOs[i].Buffer, LightingPassUBOsize, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VKCORE::WriteDescriptorSets(LogicalDevice, { UBOwrite }, { NormalTextureWrite,PositionTextureWrite});
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
    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
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
        rendererContext->CommandPool.commandPool,
        rendererContext->DeviceContext.GraphicsQueue,
        QuadVertices,
        sizeof(QuadVertices),
        QuadVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    if (EnablePhysicsDebugDrawing)
    {
        PhysicsDebugLineVertexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        MaxLines = 2000000;
        PhysicsDebugLineVertexBuffersize = MaxLines * sizeof(VKPHYSICS::DebugLineVertexInfo);
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            VKCORE::CreateBuffer(
                PhysicalDevice,
                LogicalDevice,
                PhysicsDebugLineVertexBuffersize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                PhysicsDebugLineVertexBuffer.Buffer
            );
            PhysicsDebugLineVertexBuffer.Map(LogicalDevice, 0, PhysicsDebugLineVertexBuffersize, 0);
        }
    }
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

        VkViewport Viewport{};
        Viewport.x = 0.0f;
        Viewport.y = 0.0f;
        Viewport.width = static_cast<float>(rendererContext->SwapChain.Extent.width);
        Viewport.height = static_cast<float>(rendererContext->SwapChain.Extent.height);
        Viewport.minDepth = 0.0f;
        Viewport.maxDepth = 1.0f;
        vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

        VkRect2D Scissor{};
        Scissor.offset = { 0,0 };
        Scissor.extent = rendererContext->SwapChain.Extent;
        vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

        Matrixes MatrixUBO;
        MatrixUBO.ViewMatrix = Camera.ViewMatrix;
        MatrixUBO.ProjectionMatrix = Camera.ProjectionMatrix;
        memcpy(UBOmapped[CurrentFrame], &MatrixUBO, sizeof(MatrixUBO));

        if (!Scene.Entities.empty())
        {
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

            VKCORE::TransitionImageLayout(CommandBuffer, rendererContext->SwapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            LightingPassUBOdata lightingPassUboData{};
            lightingPassUboData.CameraDirection = glm::vec4(Camera.CameraDirection, 1.0f);
            lightingPassUboData.CameraPosition = glm::vec4(Camera.CameraPosition, 1.0f);
            lightingPassUboData.StaticLightCount = static_cast<int>(Scene.StaticLights.size());
            lightingPassUboData.DynamicLightCount = static_cast<int>(Scene.DynamicLights.size());
            memcpy(LightingPassUBOs[CurrentFrame].MappedMemory, &lightingPassUboData, sizeof(LightingPassUBOdata));

            RenderLightingPass(Scene,Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);
        }
        if (EnablePhysicsDebugDrawing) RenderPhysicsDebugPass(Scene, Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);

        VKCORE::TransitionImageLayout(CommandBuffer, rendererContext->SwapChain.SwapChainImages[CurrentImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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
        rendererContext->DeviceContext.GraphicsQueue,
        rendererContext->DeviceContext.PresentQueue,
        CommandBuffers,
        { {0,RenderTask} },
        onRecreateSwapChain,
        SyncObjects,
        rendererContext->SwapChain,
        rendererContext->Window,
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
    std::array<VkClearValue, 3> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,0.0f} };
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

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)rendererContext->SwapChain.Extent.width, (uint32_t)rendererContext->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GbufferGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { Scene.SceneVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    vkCmdBindIndexBuffer(CommandBuffer, Scene.SceneIndexBuffer.BufferObject, 0, VK_INDEX_TYPE_UINT32);

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
void VKAPP::Renderer::RenderLightingPass(VKSCENE::Scene &Scene,VKSCENE::Camera3D &Camera,VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)
{
    std::array<VkClearValue, 2> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].depthStencil = { 1.0f,0 };

    VKCORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)rendererContext->SwapChain.Extent.width, (uint32_t)rendererContext->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = { LightingPassDescriptorSets[CurrentFrame],Scene.DescriptorSets[CurrentFrame] };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGraphicsPipeline.Layout, 0, 2, DescriptorSets, 0, nullptr);

    vkCmdDraw(CommandBuffer,4,1,0,0);

    RenderingPass.EndRendering(CommandBuffer);
}
void VKAPP::Renderer::RenderPhysicsDebugPass(VKSCENE::Scene& Scene, VKSCENE::Camera3D& Camera, VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)
{
    auto& DebugDrawer = Scene.DebugDrawer;
    if (!DebugDrawer) return;
    memcpy(
        PhysicsDebugLineVertexBuffers[CurrentFrame].MappedMemory,
        DebugDrawer->DebugLines.data(), 
        sizeof(VKPHYSICS::DebugLineVertexInfo) * glm::min((int)DebugDrawer->DebugLines.size(),MaxLines)
    );

    std::array<VkClearValue, 2> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].depthStencil = { 1.0f,0 };

    VKCORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );
    RenderingPass.AppendAttachment(
        DepthImage.ImageView,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)rendererContext->SwapChain.Extent.width, (uint32_t)rendererContext->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { PhysicsDebugLineVertexBuffers[CurrentFrame].Buffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.Layout, 0, 1, &GbufferPassDescriptorSets[CurrentFrame], 0, nullptr);

    vkCmdDraw(CommandBuffer,glm::min((int)DebugDrawer->DebugLines.size(), MaxLines), 1, 0, 0);

    RenderingPass.EndRendering(CommandBuffer);
};

void VKAPP::Renderer::InitializePipelines()
{
    VKCORE::ShaderModule LightingVertexShaderModule("shaders\\LightingPassShader.vert", "shaders\\LightingPassShaderVert.spv", true, LogicalDevice);
    VKCORE::ShaderModule LightingFragmentShaderModule("shaders\\LightingPassShader.frag", "shaders\\LightingPassShaderFrag.spv", true, LogicalDevice);

    VKCORE::ShaderModule GbufferVertexShaderModule("shaders\\GeometryBufferShader.vert", "shaders\\GeometryBufferShaderVert.spv", true, LogicalDevice);
    VKCORE::ShaderModule GbufferFragmentShaderModule("shaders\\GeometryBufferShader.frag", "shaders\\GeometryBufferShaderFrag.spv", true, LogicalDevice);

    VKCORE::ShaderModule PhysicsDebugVertexShaderModule("shaders\\PhysicsDebugShader.vert", "shaders\\PhysicsDebugShaderVert.spv", true, LogicalDevice);
    VKCORE::ShaderModule PhysicsDebugFragmentShaderModule("shaders\\PhysicsDebugShader.frag", "shaders\\PhysicsDebugShaderFrag.spv", true, LogicalDevice);

    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    PushConstantRange.size = sizeof(glm::mat4);
    PushConstantRange.offset = 0;

    VKCORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(rendererContext->SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(rendererContext->SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {&GbufferVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&GbufferFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 2;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R32G32B32A32_SFLOAT , VK_FORMAT_R32G32B32A32_SFLOAT };
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = DepthImageFormat;
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { rendererContext->SwapChain.Extent.width ,rendererContext->SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.AttributeDescriptions = VKSCENE::Vertex3D::GetAttributeDescriptions();
    PipelineCreateInfo.BindingDescription = VKSCENE::Vertex3D::GetBindingDescription();
    PipelineCreateInfo.DescriptorSetLayouts = { GbufferPassLayout.descriptorSetLayout };
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
    PipelineCreateInfo.DescriptorSetLayouts = {LightingPassLayout.descriptorSetLayout,rendererContext->SceneDescriptorSetLayout.descriptorSetLayout};
    PipelineCreateInfo.PushConstantRanges = {};
    LightingGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

    VKCORE::VertexInputDescription LineVertexDescription{};
    LineVertexDescription.SetBindingDescription(0, sizeof(VKPHYSICS::DebugLineVertexInfo), VK_VERTEX_INPUT_RATE_VERTEX);
    LineVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    LineVertexDescription.AppendAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);

    PipelineCreateInfo.PushConstantRanges = {};
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = DepthImageFormat;
    PipelineCreateInfo.EnableDepthTesting = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    PipelineCreateInfo.ShaderModules = { {&PhysicsDebugVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&PhysicsDebugFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DescriptorSetLayouts = {GbufferPassLayout.descriptorSetLayout};
    PipelineCreateInfo.AttributeDescriptions = LineVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = LineVertexDescription.BindingDescription;
    PhysicsDebugGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

    LightingVertexShaderModule.Destroy(LogicalDevice);
    LightingFragmentShaderModule.Destroy(LogicalDevice);
    GbufferVertexShaderModule.Destroy(LogicalDevice);
    GbufferFragmentShaderModule.Destroy(LogicalDevice);
    PhysicsDebugVertexShaderModule.Destroy(LogicalDevice);
    PhysicsDebugFragmentShaderModule.Destroy(LogicalDevice);
}

void VKAPP::Renderer::OnRecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(rendererContext->Window.window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(rendererContext->Window.window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(LogicalDevice);

    rendererContext->SwapChain.Destroy(LogicalDevice);
    DepthImage.Destroy(LogicalDevice);

    rendererContext->SwapChain.Create(PhysicalDevice, LogicalDevice, rendererContext->Surface.surface, rendererContext->Window.window);

    VKCORE::CreateImage(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = VKCORE::CreateImageView(DepthImage.Image, DepthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
        Gbuffer.Create(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);
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
    LightingPassLayout.Destroy(LogicalDevice);
    GbufferPassLayout.Destroy(LogicalDevice);
    for (auto& LightingPassUBO : LightingPassUBOs)
    {
        LightingPassUBO.Buffer.Destroy(LogicalDevice);
    }
    if (EnablePhysicsDebugDrawing)
    {
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            PhysicsDebugLineVertexBuffer.Buffer.Destroy(LogicalDevice);
        }
    }
    descriptorPool.Destroy(LogicalDevice);
    QuadVertexBuffer.Destroy(LogicalDevice);
    LightingGraphicsPipeline.Destroy(LogicalDevice);
    GbufferGraphicsPipeline.Destroy(LogicalDevice);
    PhysicsDebugGraphicsPipeline.Destroy(LogicalDevice);
};
