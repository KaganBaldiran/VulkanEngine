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

struct LightingPassUBOdata
{
    glm::vec4 CameraDirection;
    glm::vec4 CameraPosition;
    int StaticLightCount;
    int DynamicLightCount;
};

RENDERER::Renderer::Renderer(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing)
{
    Create(DestinationRendererContext, EnablePhysicsDebugDrawing);
}

void RENDERER::Renderer::Create(RendererContext& DestinationRendererContext, bool EnablePhysicsDebugDrawing)
{
    this->EnablePhysicsDebugDrawing = EnablePhysicsDebugDrawing;
    this->rendererContext = &DestinationRendererContext;

    LogicalDevice = DestinationRendererContext.DeviceContext.logicalDevice;
    PhysicalDevice = DestinationRendererContext.DeviceContext.physicalDevice;

    GraphicsQueueIndex = DestinationRendererContext.QueueFamilyIndices.GraphicsFamily.value();

    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    RENDERER_CORE::AllocateCommandBuffers(rendererContext->CommandPool.commandPool, LogicalDevice, CommandBuffers);

    Gbuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Create(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height);
    }

    //Lighting pass descriptor set
    descriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2 * MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4 * MAX_FRAMES_IN_FLIGHT} },
        2 * MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 3, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.CreateLayout(LogicalDevice);

    LightingPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    LightingPassLayouts.resize(MAX_FRAMES_IN_FLIGHT, LightingPassLayout.descriptorSetLayout);
    RENDERER_CORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, descriptorPool.descriptorPool, LightingPassLayouts, LightingPassDescriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(Gbuffers[i].AlbedoAttachment.ImageView, Gbuffers[i].AlbedoAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(Gbuffers[i].RoughnessMetallicAttachment.ImageView, Gbuffers[i].RoughnessMetallicAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { }, { NormalTextureWrite,PositionTextureWrite,AlbedoTextureWrite ,RoughnessMetallicTextureWrite });
    }

    RENDERER_CORE::CreateImage(PhysicalDevice, LogicalDevice, rendererContext->SwapChain.Extent.width, rendererContext->SwapChain.Extent.height, VK_IMAGE_TILING_OPTIMAL, rendererContext->DepthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage.Image, DepthImage.ImageMemory);
    DepthImage.ImageView = RENDERER_CORE::CreateImageView(DepthImage.Image, rendererContext->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);

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

void RENDERER::Renderer::RenderFrame(SCENE::Scene& Scene)
{
    if (!Scene.TextureManager->CurrentGbufferPassPipeline || !Scene.MeshManagerPtr)
    {
        return;
    };

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

        if (!Scene.ModelInstances.empty())
        {
            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].NormalAttachment.Image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].PositionAttachment.Image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].AlbedoAttachment.Image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].RoughnessMetallicAttachment.Image,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

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

            PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

            RenderGeometryPass(Scene, *Scene.Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].NormalAttachment.Image,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].PositionAttachment.Image,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].AlbedoAttachment.Image,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                Gbuffers[CurrentFrame].RoughnessMetallicAttachment.Image,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            PipelineBarrier2.AppendImageMemoryBarrier(
                rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

            PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

            //Lighting Pass
            RenderLightingPass(Scene, *Scene.Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);
        }
        if (EnablePhysicsDebugDrawing) RenderPhysicsDebugPass(Scene, *Scene.Camera, CommandBuffer, CurrentImageIndex, CurrentFrame);

        PipelineBarrier2.AppendImageMemoryBarrier(
            rendererContext->SwapChain.SwapChainImages[CurrentImageIndex],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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
    glfwPollEvents();
}

void RENDERER::Renderer::RenderGeometryPass(
    SCENE::Scene &Scene,
    SCENE::Camera3D &Camera,
    VkCommandBuffer& CommandBuffer, 
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame
)
{
    const size_t PerformanceModeEnabledMeshCount = Scene.MeshBuffers.PerformanceModeBuffers.EnabledMeshCount[CurrentFrame];
    if (!(PerformanceModeEnabledMeshCount)) return;

    std::array<VkClearValue, 3> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,0.0f} };
    ClearColors[1].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[2].depthStencil = { 1.0f,0 };

    RENDERER_CORE::DynamicRenderingPass RenderingPass;
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
        Gbuffers[CurrentFrame].AlbedoAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );

    RenderingPass.AppendAttachment(
        Gbuffers[CurrentFrame].RoughnessMetallicAttachment.ImageView,
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

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene.TextureManager->CurrentGbufferPassPipeline->pipeline);
    VkDescriptorSet DescriptorSets[] = { Scene.IndirectDescriptorSets[CurrentFrame],Scene.TextureManager->TexturesDescriptors[CurrentFrame].DescriptorSets[0],
        Scene.TextureIndicesDescriptorSets[CurrentFrame]};
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene.TextureManager->CurrentGbufferPassPipeline->Layout,0,3,DescriptorSets,0,nullptr);

    
    VkBuffer VertexBuffers[] = { Scene.MeshManagerPtr->GetCurrentVertexBuffer(CurrentFrame).Buffer.BufferObject};
    VkDeviceSize VertexOffsets[] = {0};
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, VertexOffsets);
    VkDeviceSize IndexOffset = 0;
    vkCmdBindIndexBuffer(CommandBuffer, Scene.MeshManagerPtr->GetCurrentIndexBuffer(CurrentFrame).Buffer.BufferObject, IndexOffset, VK_INDEX_TYPE_UINT32);
        
    glm::mat4 Matrices[2] = { Scene.Camera->ViewMatrix , Scene.Camera->ProjectionMatrix };

    vkCmdPushConstants(
        CommandBuffer,
        Scene.TextureManager->CurrentGbufferPassPipeline->Layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4) * 2,
        &Matrices
    );

    if (rendererContext->DeviceContext.DeviceFeatures.multiDrawIndirect)
    {
        vkCmdDrawIndexedIndirect(
            CommandBuffer,
            Scene.MeshBuffers.PerformanceModeBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
            0,
            PerformanceModeEnabledMeshCount,
            sizeof(SCENE::ExtendedIndirectCommand)
        );
    }
    else
    {
        for (size_t i = 0; i < PerformanceModeEnabledMeshCount; i++)
        {
            vkCmdDrawIndexedIndirect(
                CommandBuffer,
                Scene.MeshBuffers.PerformanceModeBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
                i * sizeof(SCENE::ExtendedIndirectCommand),
                1,
                sizeof(SCENE::ExtendedIndirectCommand)
            );
        }
    }
    

    RenderingPass.EndRendering(CommandBuffer);
}
void RENDERER::Renderer::RenderLightingPass(SCENE::Scene &Scene,SCENE::Camera3D &Camera,VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)
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

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPassGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { rendererContext->QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = { LightingPassDescriptorSets[CurrentFrame],Scene.SceneDescriptorSets[CurrentFrame]};
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPassGraphicsPipeline.Layout, 0, 2, DescriptorSets, 0, nullptr);

    LightingPassUBOdata lightingPassUboData{};
    lightingPassUboData.CameraDirection = glm::vec4(Scene.Camera->CameraDirection, 1.0f);
    lightingPassUboData.CameraPosition = glm::vec4(Scene.Camera->CameraPosition, 1.0f);
    lightingPassUboData.StaticLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].StaticLightLights.size());
    lightingPassUboData.DynamicLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].DynamicLights.size());

    vkCmdPushConstants(
        CommandBuffer,
        LightingPassGraphicsPipeline.Layout,
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        0,                         
        sizeof(LightingPassUBOdata),
        &lightingPassUboData
    );

    vkCmdDraw(CommandBuffer,4,1,0,0);

    RenderingPass.EndRendering(CommandBuffer);
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
    RENDERER_CORE::ShaderModule LightingVertexShaderModule("Shaders\\LightingPassShader.vert", "Shaders\\LightingPassShaderVert.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);
    RENDERER_CORE::ShaderModule LightingFragmentShaderModule("Shaders\\LightingPassShader.frag", "Shaders\\LightingPassShaderFrag.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);
    RENDERER_CORE::ShaderModule PhysicsDebugVertexShaderModule("Shaders\\PhysicsDebugShader.vert", "Shaders\\PhysicsDebugShaderVert.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);
    RENDERER_CORE::ShaderModule PhysicsDebugFragmentShaderModule("Shaders\\PhysicsDebugShader.frag", "Shaders\\PhysicsDebugShaderFrag.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, LogicalDevice);

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
    PipelineCreateInfo.ShaderModules = { {&LightingVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&LightingFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.AttributeDescriptions = rendererContext->QuadVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = rendererContext->QuadVertexDescription.BindingDescription;
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineCreateInfo.DescriptorSetLayouts = { LightingPassLayout.descriptorSetLayout,rendererContext->SceneDescriptorSetLayout.descriptorSetLayout };
    PipelineCreateInfo.PushConstantRanges = { LightingPassPushConstantRange };
    LightingPassGraphicsPipeline.Create(PipelineCreateInfo, LogicalDevice);

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

    LightingVertexShaderModule.Destroy(LogicalDevice);
    LightingFragmentShaderModule.Destroy(LogicalDevice);
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

    rendererContext->SwapChain.Create(PhysicalDevice, LogicalDevice, rendererContext->Surface.surface, rendererContext->Window.window);

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
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(Gbuffers[i].AlbedoAttachment.ImageView, Gbuffers[i].AlbedoAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(Gbuffers[i].RoughnessMetallicAttachment.ImageView, Gbuffers[i].RoughnessMetallicAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(LogicalDevice, {}, { NormalTextureWrite,PositionTextureWrite,AlbedoTextureWrite ,RoughnessMetallicTextureWrite });
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
    Gbuffers.clear();
    LightingPassLayout.Destroy(LogicalDevice);
    if (EnablePhysicsDebugDrawing)
    {
        for (auto& PhysicsDebugLineVertexBuffer : PhysicsDebugLineVertexBuffers)
        {
            PhysicsDebugLineVertexBuffer.Buffer.Destroy(LogicalDevice);
        }
    }
    descriptorPool.Destroy(LogicalDevice);
    LightingPassGraphicsPipeline.Destroy(LogicalDevice);
    PhysicsDebugGraphicsPipeline.Destroy(LogicalDevice);
    IsDestroyed = true;

    std::cout << "Renderer destroyed!" << std::endl;
};
