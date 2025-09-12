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
#include "../Scene/MaterialManager.hpp"
#include "../Scene/MeshManager.hpp"

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

    RENDERER_CORE::CreateImage(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, rendererContext->DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = RENDERER_CORE::CreateImageView(DepthImage.Image, rendererContext->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

    /*
    RENDERER_CORE::CreateImage(
        PhysicalDevice, 
        LogicalDevice, 
        rendererContext->SwapChain.Extent.width, 
        rendererContext->SwapChain.Extent.height, 
        VK_IMAGE_TILING_OPTIMAL,
        rendererContext->SwapChain.SurfaceFormat.format, 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        ColorRenderAttachmentImage.Image,
        ColorRenderAttachmentImage.ImageMemory
    );
    DepthImage.ImageView = RENDERER_CORE::CreateImageView(
        ColorRenderAttachmentImage.Image,
        rendererContext->SwapChain.SurfaceFormat.format,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_ASPECT_COLOR_BIT,
        LogicalDevice
    );
    RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, ColorRenderAttachmentImage.Sampler, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    */
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Create(PhysicalDevice, LogicalDevice, DestinationRendererContext.SwapChain.Extent.width, DestinationRendererContext.SwapChain.Extent.height);
    }

    //Lighting pass descriptor set
    DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4 * MAX_FRAMES_IN_FLIGHT} },
        MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    RENDERER_CORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.Handle, DestinationRendererContext.LightingPassLayout.Handle, LightingPassDescriptorSets.data());

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(Gbuffers[i].AlbedoAttachment.ImageView, Gbuffers[i].AlbedoAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(Gbuffers[i].RoughnessMetallicAttachment.ImageView, Gbuffers[i].RoughnessMetallicAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { }, { NormalTextureWrite,PositionTextureWrite,AlbedoTextureWrite ,RoughnessMetallicTextureWrite });
    }

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

struct MemoryBufferBarrierInfo
{
    VkPipelineStageFlags2 SrcStageMask;
    VkPipelineStageFlags2 DstStageMask;
    VkAccessFlags2 SrcAccessMask;
    VkAccessFlags2 DstAccessMask;
};

constexpr MemoryBufferBarrierInfo SceneBuffersBarrierInfos[] = {
    {   
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT 
    },
    {
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    },
    {
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    },
    {
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    },
};

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
        auto& SwapChainLayout = rendererContext->SwapChain.SwapChainImagesLayouts[CurrentImageIndex];
        if (SwapChainLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            PipelineBarrier2.AppendImageMemoryBarrier(
                rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                SwapChainLayout,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            SwapChainLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        //Transition depth image into depth attachment
        if (DepthImage.Layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        {
            PipelineBarrier2.AppendImageMemoryBarrier(
                DepthImage.Image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );
            DepthImage.Layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        }
        PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

        for (uint32_t i = 0; i < RenderPasses.size(); i++)
        {
            bool IsFirstOne = i == 0;
            RenderPassConfiguration& RenderPass = RenderPasses[i];
            auto& SceneCopyInfos = RenderPass.Scene->SceneCopyInfos[CurrentFrame];

            for (int CopySlot = 0; CopySlot < static_cast<int>(SCENE::BUFFER_COPY_SLOT_SIZE); CopySlot++)
            {
                if (!SceneCopyInfos[CopySlot].CopyRegions.empty())
                {
                    vkCmdCopyBuffer(CommandBuffer, SceneCopyInfos[CopySlot].SourceBuffer, SceneCopyInfos[CopySlot].DestinationBuffer, static_cast<uint32_t>(SceneCopyInfos[CopySlot].CopyRegions.size()), SceneCopyInfos[CopySlot].CopyRegions.data());
                    PipelineBarrier2.AppendBufferMemoryBarrier(
                        SceneCopyInfos[CopySlot].DestinationBuffer,
                        0,
                        VK_WHOLE_SIZE,
                        SceneBuffersBarrierInfos[CopySlot].SrcStageMask,
                        SceneBuffersBarrierInfos[CopySlot].DstStageMask,
                        SceneBuffersBarrierInfos[CopySlot].SrcAccessMask,
                        SceneBuffersBarrierInfos[CopySlot].DstAccessMask
                    );
                    SceneCopyInfos[CopySlot].CopyRegions.clear();
                }
            }
            PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

            auto& SceneStagingBufferAllocator = RenderPass.Scene->StagingBuffers[CurrentFrame].StagingBuffer.Allocator;
            SceneStagingBufferAllocator.Reset(SceneStagingBufferAllocator.GetCapacity());

            RenderPass.Pipeline->RenderScene(
                *RenderPass.Scene,
                CommandBuffer,
                CurrentImageIndex,
                CurrentFrame,
                DepthImage.ImageView,
                rendererContext->SwapChain.SwapChainImagesViews[CurrentImageIndex],
                Gbuffers[CurrentFrame],
                LightingPassDescriptorSets[CurrentFrame],
                RenderPass.EnableDepthTesting,
                IsFirstOne,
                IsFirstOne
            );
        }
       
        if (SwapChainLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            PipelineBarrier2.AppendImageMemoryBarrier(
                rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                0,
                SwapChainLayout,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            );
            SwapChainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

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
};

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
    PipelineCreateInfo.DescriptorSetLayouts = { rendererContext->LightingPassLayout.Handle,rendererContext->SceneDescriptorSetLayout.Handle };
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
    DepthImage.Destroy(LogicalDevice);

    rendererContext->SwapChain.Create(PhysicalDevice, LogicalDevice, rendererContext->Surface.Handle, rendererContext->Window.window);

    RENDERER_CORE::CreateImage(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, rendererContext->DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = RENDERER_CORE::CreateImageView(DepthImage.Image, rendererContext->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
        Gbuffer.Create(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(Gbuffers[i].AlbedoAttachment.ImageView, Gbuffers[i].AlbedoAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(Gbuffers[i].RoughnessMetallicAttachment.ImageView, Gbuffers[i].RoughnessMetallicAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(LogicalDevice, {}, { NormalTextureWrite,PositionTextureWrite,AlbedoTextureWrite ,RoughnessMetallicTextureWrite });
    }
    /*
    ColorRenderAttachmentImage.Destroy(LogicalDevice);
    RENDERER_CORE::CreateImage(
        PhysicalDevice,
        LogicalDevice,
        rendererContext->SwapChain.Extent.width,
        rendererContext->SwapChain.Extent.height,
        VK_IMAGE_TILING_OPTIMAL,
        rendererContext->SwapChain.SurfaceFormat.format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        ColorRenderAttachmentImage.Image,
        ColorRenderAttachmentImage.ImageMemory
    );
    DepthImage.ImageView = RENDERER_CORE::CreateImageView(
        ColorRenderAttachmentImage.Image,
        rendererContext->SwapChain.SurfaceFormat.format,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_ASPECT_COLOR_BIT,
        LogicalDevice
    );
    */

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
    DepthImage.Destroy(LogicalDevice);
  
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
    }
    DescriptorPool.Destroy(LogicalDevice);

    if (EnablePhysicsDebugDrawing)
    {
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            PhysicsDebugLineVertexBuffer.Buffer.Destroy(LogicalDevice);
        }
    }
    
    PhysicsDebugGraphicsPipeline.Destroy(LogicalDevice);
    IsDestroyed = true;
    std::cout << "Renderer destroyed!" << std::endl;
};
