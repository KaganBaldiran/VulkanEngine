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

#include "../Common/Log.hpp"
#include "MaterialManager.hpp"
#include "MeshManager.hpp"
#include "ResourceManager.hpp"

struct CullingPushConstants {
    uint32_t TotalInstanceCount;
    uint32_t Padding[3];
    glm::vec4 FrustumPlanes[6];
};

RENDERER::Renderer::Renderer(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing)
{
    Create(DestinationRendererContext, EnablePhysicsDebugDrawing);
}

void RENDERER::Renderer::Create(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing)
{
    this->EnablePhysicsDebugDrawing = EnablePhysicsDebugDrawing;
    this->rendererContext = &DestinationRendererContext;

    LogicalDevice = DestinationRendererContext.DeviceContext.LogicalDevice;
    PhysicalDevice = DestinationRendererContext.DeviceContext.PhysicalDevice;

    GraphicsQueueIndex = DestinationRendererContext.QueueFamilyIndices.GraphicsFamily.value();

    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    RENDERER_CORE::AllocateCommandBuffers(rendererContext->CommandPool.commandPool, LogicalDevice, CommandBuffers);

    //Lighting pass descriptor set
    DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,10 * MAX_FRAMES_IN_FLIGHT} },
        2 * MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    RENDERER_CORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.Handle, DestinationRendererContext.LightingPassLayout.Handle, LightingPassDescriptorSets.data());
    RENDERER_CORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.Handle, DestinationRendererContext.PostProcessDescriptorSetLayout.Handle, PostProcessingPassDescriptorSets.data());

    std::vector<RENDERER_CORE::DescriptorSetWriteImage> ImageWrites;
    ImageWrites.reserve(7 * MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        Gbuffers[i].Create(PhysicalDevice, LogicalDevice, DestinationRendererContext.SwapChain.Extent.width, DestinationRendererContext.SwapChain.Extent.height);

        RENDERER_CORE::CreateImage(
            PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width,
            rendererContext->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL, rendererContext->DepthImageFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImages[i].Image, DepthImages[i].ImageMemory
        );
        DepthImages[i].ImageView = RENDERER_CORE::CreateImageView(DepthImages[i].Image, rendererContext->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DepthImages[i].Sampler, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        //Main color attachment thats finally drawn onto 
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice,
            rendererContext->SwapChain.Extent.width,
            rendererContext->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL,
            rendererContext->SwapChain.SurfaceFormat.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ColorRenderAttachmentImages[i].Image,
            ColorRenderAttachmentImages[i].ImageMemory
        );
        ColorRenderAttachmentImages[i].ImageView = RENDERER_CORE::CreateImageView(
            ColorRenderAttachmentImages[i].Image,
            rendererContext->SwapChain.SurfaceFormat.format,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_ASPECT_COLOR_BIT,
            LogicalDevice
        );
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, ColorRenderAttachmentImages[i].Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);


        //Gbuffer attachments descriptor writes
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(
            Gbuffers[i].PositionAttachment.ImageView,
            Gbuffers[i].PositionAttachment.Sampler, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            0,
            LightingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(
            Gbuffers[i].NormalAttachment.ImageView,
            Gbuffers[i].NormalAttachment.Sampler, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            1, 
            LightingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(
            Gbuffers[i].AlbedoAttachment.ImageView, 
            Gbuffers[i].AlbedoAttachment.Sampler, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            2, 
            LightingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(
            Gbuffers[i].RoughnessMetallicAttachment.ImageView, 
            Gbuffers[i].RoughnessMetallicAttachment.Sampler, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            3, 
            LightingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        ImageWrites.push_back(std::move(PositionTextureWrite));
        ImageWrites.push_back(std::move(NormalTextureWrite));
        ImageWrites.push_back(std::move(AlbedoTextureWrite));
        ImageWrites.push_back(std::move(RoughnessMetallicTextureWrite));
    

        //Post process pass attachments descriptor writes
        RENDERER_CORE::DescriptorSetWriteImage ColorRenderAttachmentTextureWrite(
            ColorRenderAttachmentImages[i].ImageView,
            ColorRenderAttachmentImages[i].Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            0, 
            PostProcessingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage DepthTextureWrite(
            DepthImages[i].ImageView,
            DepthImages[i].Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            1, 
            PostProcessingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage PostProcessNormalTextureWrite(
            Gbuffers[i].NormalAttachment.ImageView, 
            Gbuffers[i].NormalAttachment.Sampler, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            2, 
            PostProcessingPassDescriptorSets[i], 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        ImageWrites.push_back(std::move(ColorRenderAttachmentTextureWrite));
        ImageWrites.push_back(std::move(DepthTextureWrite));
        ImageWrites.push_back(std::move(PostProcessNormalTextureWrite));
    }
    RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { }, ImageWrites);

    InitializePipelines();

    RENDERER_CORE::FrameSyncObjects SyncObject;
    SyncObject.FenceCreateFlag = VK_FENCE_CREATE_SIGNALED_BIT;
    SyncObjects.resize(MAX_FRAMES_IN_FLIGHT, SyncObject);
    RENDERER_CORE::AllocateFrameSyncObjects(LogicalDevice, SyncObjects);

    if (EnablePhysicsDebugDrawing)
    {
        PhysicsDebugLineVertexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        MaxLines = 2000000;
        PhysicsDebugLineVertexBuffersize = MaxLines * sizeof(VKPHYSICS::DebugLineVertexInfo);
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            RENDERER_CORE::CreateBuffer(
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

    CreateTestResources();

    this->IsDestroyed = false;
    this->DestructionPriority = 1;
    COMMON::DestructionQueue::Get()->Register(this);

    COMMON::LogMessage Message{};
    Message.Severity = COMMON::LOG_SEVERITY_INFO;
    Message.Message = "Renderer initialized!";

    COMMON::Logger::Get().FileLog("Log.txt", Message);
    COMMON::Logger::Get().ConsoleLog(Message);
}

void RENDERER::Renderer::AddRenderPass(RenderPassConfiguration RenderPass)
{
    auto RenderPassIterator = std::find_if(RenderPasses.begin(), RenderPasses.end(), [&RenderPass](RenderPassConfiguration& Pass) {
        return Pass.Name == RenderPass.Name;
    });
    
    if (RenderPassIterator == RenderPasses.end())
    {
        RenderPasses.push_back(RenderPass);
    }
    else
    {
        RenderPassIterator->Pipeline = RenderPass.Pipeline;
        RenderPassIterator->Scene = RenderPass.Scene;
        RenderPassIterator->EnableDepthTesting = RenderPass.EnableDepthTesting;
    }
}

void RENDERER::Renderer::RemoveRenderPass(std::string RenderPassName)
{
    auto RenderPassIterator = std::find_if(RenderPasses.begin(), RenderPasses.end(), [&RenderPassName](RenderPassConfiguration& Pass) {
           return Pass.Name == RenderPassName;
    });
    if (RenderPassIterator != RenderPasses.end()) RenderPasses.erase(RenderPassIterator);
}

void RENDERER::Renderer::RenderFrame()
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

        //Transition the current swap chain image into color attachment
        auto& SwapChainBarrierState = rendererContext->SwapChain.SwapChainImagesBarrierStates[CurrentImageIndex];
        RENDERER_CORE::SafeImageBarrier(
            rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
            SwapChainBarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        );

        RENDERER_CORE::SafeImageBarrier(
            ColorRenderAttachmentImages[CurrentFrame].Image,
            ColorRenderAttachmentImages[CurrentFrame].BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        );

        RENDERER_CORE::SafeImageBarrier(
            DepthImages[CurrentFrame].Image,
            DepthImages[CurrentFrame].BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
        //Resource copying passes
        for (uint32_t i = 0; i < RenderPasses.size(); i++)
        {
            RenderPasses[i].Scene->ResourceManagerPtr->HandleCopyOperations(CommandBuffer, CurrentFrame, PipelineBarrier2);
        }
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        bool EnableCulling = true;
        //Passes that reset the culled buffers
        for (uint32_t i = 0; i < RenderPasses.size(); i++)
        {
            RenderPassConfiguration& RenderPass = RenderPasses[i];
            DispatchComputeResetCulling(
                *RenderPass.Scene,
                *RenderPass.Scene->Camera,
                CommandBuffer,
                CurrentImageIndex,
                CurrentFrame,
                PipelineBarrier2,
                EnableCulling
            );
        }
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        //Culling passes
        for (uint32_t i = 0; i < RenderPasses.size(); i++)
        {
            RenderPassConfiguration& RenderPass = RenderPasses[i];
            DispatchComputeCulling(
                *RenderPass.Scene,
                *RenderPass.Scene->Camera,
                CommandBuffer,
                CurrentImageIndex,
                CurrentFrame,
                PipelineBarrier2,
                EnableCulling
            );
        }
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        //Rendering passes
        for (uint32_t i = 0; i < RenderPasses.size(); i++)
        {
            bool IsFirstOne = i == 0;
            RenderPassConfiguration& RenderPass = RenderPasses[i];
            RenderPass.Pipeline->RenderScene(
                *RenderPass.Scene,
                CommandBuffer,
                CurrentImageIndex,
                CurrentFrame,
                DepthImages[CurrentFrame].ImageView,
                //rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
                ColorRenderAttachmentImages[CurrentFrame].ImageView,
                Gbuffers[CurrentFrame],
                LightingPassDescriptorSets[CurrentFrame],
                RenderPass.EnableDepthTesting,
                IsFirstOne,
                IsFirstOne
            );
        }

        RENDERER_CORE::SafeImageBarrier(
            ColorRenderAttachmentImages[CurrentFrame].Image,
            ColorRenderAttachmentImages[CurrentFrame].BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
        );
        
        //Transition depth image into depth attachment
        RENDERER_CORE::SafeImageBarrier(
            DepthImages[CurrentFrame].Image,
            DepthImages[CurrentFrame].BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        //Post process pass that renders the final rendered image onto the swapchain image with effects 
        RenderPostProcessPass(SCENE::Camera3D(), CommandBuffer, CurrentImageIndex, CurrentFrame);
       
        RENDERER_CORE::SafeImageBarrier(
            rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
            SwapChainBarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            0
        );
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer");
        }
    };

    auto onRecreateSwapChain = [&]()
    {
       OnRecreateSwapChain();
    };

    RENDERER_CORE::RenderFrame(
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

}

void RENDERER::Renderer::RenderPhysicsDebugPass(SCENE::Scene& Scene, SCENE::Camera3D& Camera, VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)
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

    RENDERER_CORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );
    RenderingPass.AppendAttachment(
        DepthImages[CurrentFrame].ImageView,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)rendererContext->SwapChain.Extent.width, (uint32_t)rendererContext->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.Handle);

    VkBuffer VertexBuffers[] = { PhysicsDebugLineVertexBuffers[CurrentFrame].Buffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    //vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.Layout, 0, 1, &GbufferPassDescriptorSets[CurrentFrame], 0, nullptr);

    glm::mat4 Matrices[2] = { Scene.Camera->ViewMatrix , Scene.Camera->ProjectionMatrix };

    vkCmdPushConstants(
        CommandBuffer,
        PhysicsDebugGraphicsPipeline.Layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &Scene.Camera->ViewMatrix
    );

    vkCmdDraw(CommandBuffer,glm::min((int)DebugDrawer->DebugLines.size(), MaxLines), 1, 0, 0);

    RenderingPass.EndRendering(CommandBuffer);
}
void RENDERER::Renderer::RenderPostProcessPass(
    SCENE::Camera3D& Camera, 
    VkCommandBuffer& CommandBuffer, 
    uint32_t CurrentImageIndex, 
    uint32_t CurrentFrame
)
{
    std::array<VkClearValue, 2> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].depthStencil = { 1.0f,0 };

    RENDERER_CORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)rendererContext->SwapChain.Extent.width, (uint32_t)rendererContext->SwapChain.Extent.height} });

    size_t CurrentPipelineIndex = rendererContext->DefaultPipelines.PostProcessing;
    RENDERER_CORE::GraphicsPipelineEntry* CurrentPipelineEntry = rendererContext->PipelineManager.GetGraphicsPipeline(CurrentPipelineIndex);
    assert(CurrentPipelineEntry);
    RENDERER_CORE::GraphicsPipeline CurrentPipeline = CurrentPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Handle);

    VkBuffer VertexBuffers[] = { rendererContext->QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = {
        PostProcessingPassDescriptorSets[CurrentFrame]
    };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 1, DescriptorSets, 0, nullptr);

    PostProcessingPassPushConstantData PushConstantData{};
    PushConstantData.CameraDirection = Camera.CameraDirection;
    PushConstantData.CameraPosition = Camera.CameraPosition;
    PushConstantData.FogIntensity = 0.4f;
    PushConstantData.CameraFrustumLength = Camera.FarPlane - Camera.NearPlane;
   // PushConstantData.Time = std::chrono::duration<float>(std::chrono::system_clock::now()).count();
    PushConstantData.Time = glfwGetTime();

    vkCmdPushConstants(
        CommandBuffer,
        CurrentPipeline.Layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PostProcessingPassPushConstantData),
        &PushConstantData
    );

    vkCmdDraw(CommandBuffer, 4, 1, 0, 0);
    RenderingPass.EndRendering(CommandBuffer);
}

void RENDERER::Renderer::CreateTestResources()
{
    TestDescriptor.Layout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_COMPUTE_BIT);
    TestDescriptor.Layout.CreateLayout(LogicalDevice);

    TestDescriptor.DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1} },
        1, LogicalDevice
    );

    RENDERER_CORE::AllocateDescriptorSets(
        LogicalDevice, 
        1, 
        TestDescriptor.DescriptorPool.Handle,
        TestDescriptor.Layout.Handle, 
        TestDescriptor.DescriptorSets.data()
    );

    rendererContext->ShaderManager.AppendShaderModule(
        "TestComputeShader",
        "Shaders\\TestComputeShader.comp",
        "Shaders\\TestComputeShader.spv",
        shaderc_compute_shader,
        LogicalDevice
    );

    RENDERER_CORE::ComputePipelineCreateInfo Info{};
    Info.DescriptorSetLayouts = { TestDescriptor.Layout };
    Info.ComputeShaderModule = rendererContext->ShaderManager.GetShaderModule("TestComputeShader");
    TestPipeline.Create(Info,LogicalDevice);

    size_t BufferSize = sizeof(int) * 5;

    RENDERER_CORE::CreateBuffer(
        PhysicalDevice, 
        LogicalDevice,
        BufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        TestBuffer.Buffer
    );
    TestBuffer.Map(LogicalDevice, 0, BufferSize, 0);

    RENDERER_CORE::DescriptorSetWriteBuffer BufferWrite{};
    BufferWrite.Create(TestBuffer.Buffer, BufferSize, 0, TestDescriptor.DescriptorSets[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { BufferWrite }, {});
}

void RENDERER::Renderer::DestroyTestResources()
{
    TestPipeline.Destroy(LogicalDevice);
    TestDescriptor.Destroy(LogicalDevice);
    rendererContext->ShaderManager.EraseShaderModule("TestComputeShader", LogicalDevice);
    TestBuffer.Destroy(LogicalDevice);
}

void RENDERER::Renderer::DispatchComputeTest(
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame
)
{
    if (!ShouldTest) {
        std::array<int, 5> Data;
        memcpy(Data.data(), TestBuffer.MappedMemory, sizeof(int) * 5);
        /*std::cout << "Test Data: ";
        for (size_t i = 0; i < Data.size(); i++)
        {
            std::cout << Data[i] << " ";
        }
        std::cout << std::endl;*/
        return;
    };
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, TestPipeline.Handle); 
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, TestPipeline.Layout, 0, 1, &TestDescriptor.DescriptorSets[0], 0, nullptr);
    vkCmdDispatch(CommandBuffer, 1, 1, 1);
    ShouldTest = false;
}

void RENDERER::Renderer::DispatchComputeCulling(
    SCENE::Scene& Scene,
    SCENE::Camera3D& Camera,
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame,
    RENDERER_CORE::PipelineBarrier2 &PipelineBarrier2,
    bool EnableCulling
)
{
    auto& MeshBuffers = Scene.MeshBuffers;
    uint32_t EnabledMeshCount = MeshBuffers.SceneBuffers.EnabledMeshCount[CurrentFrame];
    uint32_t IndirectCommandCount = MeshBuffers.Entries[CurrentFrame].MeshEntries.Size();
    if (!IndirectCommandCount || !EnabledMeshCount || !EnableCulling) return;
    //std::cout << "CurrentFrame: " << CurrentFrame << " EnabledMeshCount: " << EnabledMeshCount << " IndirectCommandCount: " << IndirectCommandCount << std::endl;
    int WorkGroupSize = 64;

    /*uint32_t CullingResetWorkGroupCount = (IndirectCommandCount + WorkGroupSize - 1) / WorkGroupSize;
    const size_t CullingResetPipelineIndex = rendererContext->DefaultPipelines.CullingResetCompute;
    RENDERER_CORE::ComputePipelineEntry* CullingResetPipelineEntry = rendererContext->PipelineManager.GetComputePipeline(CullingResetPipelineIndex);
    assert(CullingResetPipelineEntry);
    RENDERER_CORE::ComputePipeline& CullingResetPipeline = CullingResetPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingResetPipeline.Handle);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingResetPipeline.Layout, 0, 1, &Scene.IndirectDescriptorSets[CurrentFrame], 0, nullptr);

    vkCmdPushConstants(
        CommandBuffer,
        CullingResetPipeline.Layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(uint32_t),
        &IndirectCommandCount
    );

    vkCmdDispatch(CommandBuffer, CullingResetWorkGroupCount, 1, 1);

    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
    );
    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);*/

    uint32_t CullingWorkGroupCount = (EnabledMeshCount + WorkGroupSize - 1) / WorkGroupSize;
    const size_t CullingPipelineIndex = rendererContext->DefaultPipelines.CullingCompute;
    RENDERER_CORE::ComputePipelineEntry* CullingPipelineEntry = rendererContext->PipelineManager.GetComputePipeline(CullingPipelineIndex);
    assert(CullingPipelineEntry);
    RENDERER_CORE::ComputePipeline& CullingPipeline = CullingPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingPipeline.Handle);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingPipeline.Layout, 0, 1, &Scene.IndirectDescriptorSets[CurrentFrame], 0, nullptr);

    /*std::array<glm::vec4, 7> CullingPushData;
    std::array<glm::vec4, 6> FrustumPlanes;
    Camera.ExtractFrustumPlanes(FrustumPlanes);
    memcpy(CullingPushData.data() + 1, FrustumPlanes.data(), sizeof(glm::vec4) * 6);
    CullingPushData[0].x = EnabledMeshCount;*/

    CullingPushConstants PushData{};
    PushData.TotalInstanceCount = static_cast<uint32_t>(EnabledMeshCount);

    std::array<glm::vec4, 6> Planes;
    Camera.ExtractFrustumPlanes(Planes);
    memcpy(PushData.FrustumPlanes, Planes.data(), sizeof(glm::vec4) * 6);

    vkCmdPushConstants(
        CommandBuffer, 
        CullingPipeline.Layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0, 
        sizeof(CullingPushConstants), 
        &PushData
    );

   /* vkCmdPushConstants(
        CommandBuffer,
        CullingPipeline.Layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(glm::vec4) * 7,
        CullingPushData.data()
    );*/

    vkCmdDispatch(CommandBuffer, CullingWorkGroupCount, 1, 1);

    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.ModelMatricesBuffers[CurrentFrame].Buffer.Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.DrawMetaDataBuffer[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.CullBuffers.VisibilityIndexBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    //PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);
}

void RENDERER::Renderer::DispatchComputeResetCulling(SCENE::Scene& Scene, SCENE::Camera3D& Camera, VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame, RENDERER_CORE::PipelineBarrier2& PipelineBarrier2, bool EnableCulling)
{
    auto& MeshBuffers = Scene.MeshBuffers;
    uint32_t EnabledMeshCount = MeshBuffers.SceneBuffers.EnabledMeshCount[CurrentFrame];
    uint32_t IndirectCommandCount = MeshBuffers.Entries[CurrentFrame].MeshEntries.Size();
    if (!IndirectCommandCount || !EnabledMeshCount || !EnableCulling) return;
    //std::cout << "CurrentFrame: " << CurrentFrame << " EnabledMeshCount: " << EnabledMeshCount << " IndirectCommandCount: " << IndirectCommandCount << std::endl;
    int WorkGroupSize = 64;

    uint32_t CullingResetWorkGroupCount = (IndirectCommandCount + WorkGroupSize - 1) / WorkGroupSize;
    const size_t CullingResetPipelineIndex = rendererContext->DefaultPipelines.CullingResetCompute;
    RENDERER_CORE::ComputePipelineEntry* CullingResetPipelineEntry = rendererContext->PipelineManager.GetComputePipeline(CullingResetPipelineIndex);
    assert(CullingResetPipelineEntry);
    RENDERER_CORE::ComputePipeline& CullingResetPipeline = CullingResetPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingResetPipeline.Handle);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingResetPipeline.Layout, 0, 1, &Scene.IndirectDescriptorSets[CurrentFrame], 0, nullptr);

    vkCmdPushConstants(
        CommandBuffer,
        CullingResetPipeline.Layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(uint32_t),
        &IndirectCommandCount
    );

    vkCmdDispatch(CommandBuffer, CullingResetWorkGroupCount, 1, 1);

    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.SceneBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
    );
}

void RENDERER::Renderer::InitializePipelines()
{
   // RENDERER_CORE::ShaderModule LightingVertexShaderModule("Shaders\\LightingPassShader.vert", "Shaders\\LightingPassShaderVert.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);
    //RENDERER_CORE::ShaderModule LightingFragmentShaderModule("Shaders\\LightingPassShader.frag", "Shaders\\LightingPassShaderFrag.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);
    RENDERER_CORE::ShaderModule PhysicsDebugVertexShaderModule;
    PhysicsDebugVertexShaderModule.CompileOrLoadShaderModule(
        "Shaders\\PhysicsDebugShader.vert",
        "Shaders\\PhysicsDebugShaderVert.spv",
        shaderc_vertex_shader,
        "PhysicsDebugVertexShaderModule",
        LogicalDevice
    );

    RENDERER_CORE::ShaderModule PhysicsDebugFragmentShaderModule;
    PhysicsDebugFragmentShaderModule.CompileOrLoadShaderModule(
        "Shaders\\PhysicsDebugShader.frag",
        "Shaders\\PhysicsDebugShaderFrag.spv",
        shaderc_fragment_shader,
        "PhysicsDebugFragmentShaderModule",
        LogicalDevice
    );

    VkPushConstantRange LightingPassPushConstantRange{};
    LightingPassPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    LightingPassPushConstantRange.size = sizeof(LightingPassUBOdata);
    LightingPassPushConstantRange.offset = 0;

    RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(rendererContext->SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(rendererContext->SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { rendererContext->SwapChain.Extent.width ,rendererContext->SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R8G8B8A8_SRGB };
   // PipelineCreateInfo.ShaderModules = { {&LightingVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&LightingFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.AttributeDescriptions = rendererContext->QuadVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = rendererContext->QuadVertexDescription.BindingDescription;
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineCreateInfo.DescriptorSetLayouts = { rendererContext->LightingPassLayout,rendererContext->SceneDescriptorSetLayout };
    PipelineCreateInfo.PushConstantRanges = { LightingPassPushConstantRange };
//    LightingPassGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

    RENDERER_CORE::VertexInputDescription LineVertexDescription{};
    LineVertexDescription.SetBindingDescription(0, sizeof(VKPHYSICS::DebugLineVertexInfo), VK_VERTEX_INPUT_RATE_VERTEX);
    LineVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    LineVertexDescription.AppendAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);

    VkPushConstantRange PhysicsDebugPassPushConstantRange{};
    PhysicsDebugPassPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PhysicsDebugPassPushConstantRange.size = sizeof(glm::mat4);
    PhysicsDebugPassPushConstantRange.offset = 0;

    PipelineCreateInfo.PushConstantRanges = { PhysicsDebugPassPushConstantRange };
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = rendererContext->DepthImageFormat;
    PipelineCreateInfo.EnableDepthTesting = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    PipelineCreateInfo.ShaderModules = { {&PhysicsDebugVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&PhysicsDebugFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DescriptorSetLayouts = {};
    PipelineCreateInfo.AttributeDescriptions = LineVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = LineVertexDescription.BindingDescription;
    PhysicsDebugGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

   // LightingVertexShaderModule.Destroy(LogicalDevice);
    //LightingFragmentShaderModule.Destroy(LogicalDevice);
    PhysicsDebugVertexShaderModule.Destroy(LogicalDevice);
    PhysicsDebugFragmentShaderModule.Destroy(LogicalDevice);
}

void RENDERER::Renderer::OnRecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(rendererContext->Window.window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(rendererContext->Window.window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(LogicalDevice);

    rendererContext->SwapChain.Destroy(LogicalDevice);
    rendererContext->SwapChain.Create(PhysicalDevice, LogicalDevice, rendererContext->Surface.Handle, rendererContext->Window.window);

    std::vector<RENDERER_CORE::DescriptorSetWriteImage> ImageWrites;
    ImageWrites.reserve(7 * MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        //Gbuffer recreation
        Gbuffers[i].Destroy(LogicalDevice);
        Gbuffers[i].Create(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);

        //Depth image recreation
        DepthImages[i].Destroy(LogicalDevice);
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice, 
            rendererContext->SwapChain.Extent.width, 
            rendererContext->SwapChain.Extent.height, 
            VK_IMAGE_TILING_OPTIMAL, 
            rendererContext->DepthImageFormat, 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            DepthImages[i].Image, 
            DepthImages[i].ImageMemory
        );
        DepthImages[i].ImageView = RENDERER_CORE::CreateImageView(DepthImages[i].Image, rendererContext->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DepthImages[i].Sampler, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        //Main render attachment recreation
        ColorRenderAttachmentImages[i].Destroy(LogicalDevice);
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice,
            rendererContext->SwapChain.Extent.width,
            rendererContext->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL,
            rendererContext->SwapChain.SurfaceFormat.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ColorRenderAttachmentImages[i].Image,
            ColorRenderAttachmentImages[i].ImageMemory
        );
        ColorRenderAttachmentImages[i].ImageView = RENDERER_CORE::CreateImageView(
            ColorRenderAttachmentImages[i].Image,
            rendererContext->SwapChain.SurfaceFormat.format,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_ASPECT_COLOR_BIT,
            LogicalDevice
        );
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, ColorRenderAttachmentImages[i].Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(
            Gbuffers[i].PositionAttachment.ImageView,
            Gbuffers[i].PositionAttachment.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0, LightingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(
            Gbuffers[i].NormalAttachment.ImageView,
            Gbuffers[i].NormalAttachment.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1, LightingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(
            Gbuffers[i].AlbedoAttachment.ImageView,
            Gbuffers[i].AlbedoAttachment.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            2,
            LightingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(
            Gbuffers[i].RoughnessMetallicAttachment.ImageView,
            Gbuffers[i].RoughnessMetallicAttachment.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            3,
            LightingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        ImageWrites.push_back(std::move(PositionTextureWrite));
        ImageWrites.push_back(std::move(NormalTextureWrite));
        ImageWrites.push_back(std::move(AlbedoTextureWrite));
        ImageWrites.push_back(std::move(RoughnessMetallicTextureWrite));

        //Post process pass attachments descriptor writes
        RENDERER_CORE::DescriptorSetWriteImage ColorRenderAttachmentTextureWrite(
            ColorRenderAttachmentImages[i].ImageView,
            ColorRenderAttachmentImages[i].Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            PostProcessingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage DepthTextureWrite(
            DepthImages[i].ImageView,
            DepthImages[i].Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1,
            PostProcessingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        RENDERER_CORE::DescriptorSetWriteImage PostProcessNormalTextureWrite(
            Gbuffers[i].NormalAttachment.ImageView,
            Gbuffers[i].NormalAttachment.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            2,
            PostProcessingPassDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );
        ImageWrites.push_back(std::move(ColorRenderAttachmentTextureWrite));
        ImageWrites.push_back(std::move(DepthTextureWrite));
        ImageWrites.push_back(std::move(PostProcessNormalTextureWrite));
    }
    RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { }, ImageWrites);

    for (uint32_t i = 0; i < RenderPasses.size(); i++)
    {
        RenderPassConfiguration& RenderPass = RenderPasses[i];
        RenderPass.Pipeline->OnResize(rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);
    }
}

void RENDERER::Renderer::Destroy()
{
    if (IsDestroyed) return;

    RENDERER_CORE::DestroyFrameSyncObjects(LogicalDevice, SyncObjects);
  
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        DepthImages[i].Destroy(LogicalDevice);
        ColorRenderAttachmentImages[i].Destroy(LogicalDevice);
        Gbuffers[i].Destroy(LogicalDevice);
    }
    DescriptorPool.Destroy(LogicalDevice);

    if (EnablePhysicsDebugDrawing)
    {
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            PhysicsDebugLineVertexBuffer.Buffer.Destroy(LogicalDevice);
        }
    }
    
    DestroyTestResources();

    PhysicsDebugGraphicsPipeline.Destroy(LogicalDevice);
    IsDestroyed = true;
    std::cout << "Renderer destroyed!" << std::endl;
};
