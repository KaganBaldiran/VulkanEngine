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
#include <chrono>

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
    this->RendererContextPtr = &DestinationRendererContext;

    LogicalDevice = DestinationRendererContext.DeviceContext.LogicalDevice;
    PhysicalDevice = DestinationRendererContext.DeviceContext.PhysicalDevice;
    GraphicsQueueIndex = DestinationRendererContext.QueueFamilyIndices.GraphicsFamily.value();

    CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    RENDERER_CORE::AllocateCommandBuffers(RendererContextPtr->CommandPool.Handle, LogicalDevice, CommandBuffers);

    FrameManager.Create(RendererContextPtr->DeviceContext.LogicalDevice, RendererContextPtr->CommandPool.Handle);
    TimelineSemaphore.Create(RendererContextPtr->DeviceContext.LogicalDevice, 0);

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
            PhysicalDevice, LogicalDevice, RendererContextPtr->SwapChain.Extent.width,
            RendererContextPtr->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL, RendererContextPtr->DepthImageFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImages[i].Image, DepthImages[i].ImageMemory
        );
        DepthImages[i].ImageView = RENDERER_CORE::CreateImageView(DepthImages[i].Image, RendererContextPtr->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DepthImages[i].Sampler, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        //Main color attachment thats finally drawn onto 
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice,
            RendererContextPtr->SwapChain.Extent.width,
            RendererContextPtr->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL,
            RendererContextPtr->SwapChain.SurfaceFormat.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ColorRenderAttachmentImages[i].Image,
            ColorRenderAttachmentImages[i].ImageMemory
        );
        ColorRenderAttachmentImages[i].ImageView = RENDERER_CORE::CreateImageView(
            ColorRenderAttachmentImages[i].Image,
            RendererContextPtr->SwapChain.SurfaceFormat.format,
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
                PhysicsDebugLineVertexBuffer
            );
            RENDERER_CORE::MapBuffer(PhysicsDebugLineVertexBuffer, LogicalDevice, 
                                          0, PhysicsDebugLineVertexBuffersize, 0);
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
        UniqueSceneCameraPairs.insert({ RenderPass.Scene->GetHandleID() ,
                                     RenderPass.Camera->GetHandleID() }, { RenderPass.Scene,RenderPass.Camera });
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
    if (RenderPassIterator != RenderPasses.end()) 
    { 
        UniqueSceneCameraPairs.erase({ RenderPassIterator->Scene->GetHandleID(), RenderPassIterator->Camera->GetHandleID() });
        RenderPasses.erase(RenderPassIterator); 
    }
}

/*
void RENDERER::Renderer::RenderFrame()
{
    VkCommandBuffer MainCommandBuffer = FrameManager.BeginFrame(RendererContextPtr->DeviceContext.LogicalDevice, RendererContextPtr->SwapChain.Handle,TimelineSemaphore);
    if (MainCommandBuffer == VK_NULL_HANDLE)
    {
        OnRecreateSwapChain();
        return;
    }
    uint32_t CurrentImageIndex = FrameManager.ImageIndex;
    RENDERER_CORE::BeginCommandBuffer(MainCommandBuffer);

    VkViewport Viewport1{};
    Viewport1.x = 0.0f;
    Viewport1.y = 0.0f;
    Viewport1.width = static_cast<float>(RendererContextPtr->SwapChain.Extent.width);
    Viewport1.height = static_cast<float>(RendererContextPtr->SwapChain.Extent.height);
    Viewport1.minDepth = 0.0f;
    Viewport1.maxDepth = 1.0f;

    VkRect2D Scissor1{};
    Scissor1.offset = { 0,0 };
    Scissor1.extent = { RendererContextPtr->SwapChain.Extent.width ,RendererContextPtr->SwapChain.Extent.height };

    //vkCmdSetViewport(MainCommandBuffer, 0, 1, &Viewport1);
    //vkCmdSetScissor(MainCommandBuffer, 0, 1, &Scissor1);

    auto& CurrentSyncObjects = FrameManager.SyncObjects[CurrentFrame];

    GlobalTimelineCounter++;
    CurrentSyncObjects.TimelineCounterTarget = GlobalTimelineCounter;

    //Transition the current swap chain image into color attachment
    auto& SwapChainBarrierState = RendererContextPtr->SwapChain.SwapChainImagesBarrierStates[CurrentImageIndex];
    RENDERER_CORE::SafeImageBarrier(
        RendererContextPtr->SwapChain.SwapChainImages[CurrentImageIndex],
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
        RenderPasses[i].Scene->ResourceManagerPtr->HandleCopyOperations(MainCommandBuffer, CurrentFrame, PipelineBarrier2);
    }
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);

    bool EnableCulling = true;

    //Passes that reset the culled buffers
    for (auto SceneCameraPair : UniqueSceneCameraPairs)
    {
        DispatchComputeResetCulling(
            *SceneCameraPair.second.first,
            *SceneCameraPair.second.second,
            MainCommandBuffer,
            CurrentImageIndex,
            CurrentFrame,
            PipelineBarrier2,
            EnableCulling
        );
    }
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);

    //Culling passes
    for (auto SceneCameraPair : UniqueSceneCameraPairs)
    {
        DispatchComputeCulling(
            *SceneCameraPair.second.first,
            *SceneCameraPair.second.second,
            MainCommandBuffer,
            CurrentImageIndex,
            CurrentFrame,
            PipelineBarrier2,
            EnableCulling
        );
    }
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);

    //Rendering passes
    for (uint32_t i = 0; i < RenderPasses.size(); i++)
    {
        bool IsFirstOne = i == 0;
        RenderPassConfiguration& RenderPass = RenderPasses[i];

        vkCmdSetViewport(MainCommandBuffer, 0, 1, &RenderPass.Viewport);
        vkCmdSetScissor(MainCommandBuffer, 0, 1, &RenderPass.Scissor);

        RenderPass.Pipeline->RenderScene(
            *RenderPass.Scene,
            *RenderPass.Camera,
            MainCommandBuffer,
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
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);

    //Post process pass that renders the final rendered image onto the swapchain image with effects 
    RenderPostProcessPass(SCENE::Camera3D(), MainCommandBuffer, CurrentImageIndex, CurrentFrame);
       
    RENDERER_CORE::SafeImageBarrier(
        RendererContextPtr->SwapChain.SwapChainImages[CurrentImageIndex],
        SwapChainBarrierState,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0
    );
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);

    if (vkEndCommandBuffer(MainCommandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer");
    }

    uint64_t SignalValues[] = {0,GlobalTimelineCounter};
    VkTimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo = RENDERER_CORE::TimelineSemaphoreSubmitInfo(nullptr, 0, SignalValues, 2);

    RENDERER_CORE::SubmitQueue(
        RendererContextPtr->DeviceContext.GraphicsQueue,
        { CurrentSyncObjects.ImageAvailableSemaphore },
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        { MainCommandBuffer },
        { CurrentSyncObjects.RenderFinishedSemaphore , TimelineSemaphore.Handle },
        nullptr,
        &TimelineSemaphoreSubmitInfo
    );
  
    VkResult Result = FrameManager.EndFrame(
        RendererContextPtr->DeviceContext.LogicalDevice,
        RendererContextPtr->DeviceContext.PresentQueue,
        RendererContextPtr->SwapChain.Handle,
        RendererContextPtr->Window
    );
    if (Result != VK_SUCCESS)
    {
        OnRecreateSwapChain();
        return;
    }
    CurrentFrame = FrameManager.CurrentFrame;
}
*/

void RENDERER::Renderer::RenderFrame()
{
    VkCommandBuffer MainCommandBuffer = FrameManager.BeginFrame(RendererContextPtr->DeviceContext.LogicalDevice, RendererContextPtr->SwapChain.Handle, TimelineSemaphore);
    if (MainCommandBuffer == VK_NULL_HANDLE)
    {
        OnRecreateSwapChain();
        return;
    }
    uint32_t CurrentImageIndex = FrameManager.ImageIndex;
    RENDERER_CORE::BeginCommandBuffer(MainCommandBuffer);

    VkViewport Viewport1{};
    Viewport1.x = 0.0f;
    Viewport1.y = 0.0f;
    Viewport1.width = static_cast<float>(RendererContextPtr->SwapChain.Extent.width);
    Viewport1.height = static_cast<float>(RendererContextPtr->SwapChain.Extent.height);
    Viewport1.minDepth = 0.0f;
    Viewport1.maxDepth = 1.0f;

    VkRect2D Scissor1{};
    Scissor1.offset = { 0,0 };
    Scissor1.extent = { RendererContextPtr->SwapChain.Extent.width ,RendererContextPtr->SwapChain.Extent.height };

    auto& CurrentSyncObjects = FrameManager.SyncObjects[CurrentFrame];

    GlobalTimelineCounter++;
    CurrentSyncObjects.TimelineCounterTarget = GlobalTimelineCounter;

    //Transition the current swap chain image into color attachment
    auto& SwapChainBarrierState = RendererContextPtr->SwapChain.SwapChainImagesBarrierStates[CurrentImageIndex];
    auto& SwapChainImageHandle = RendererContextPtr->SwapChain.SwapChainImages[CurrentImageIndex];
    RENDERER_CORE::ImageData SwapChainImage;
    SwapChainImage.BarrierState = SwapChainBarrierState;
    SwapChainImage.Image = SwapChainImageHandle;

    for (auto &[HandlePair,SceneCameraPair] : UniqueSceneCameraPairs)
    {
        SCENE::Scene* Scene = SceneCameraPair.first;
        Scene->ResourceManagerPtr->MeshManager.UpdateGeometryEntries();
        Scene->ResourceManagerPtr->TextureManager.UpdateDescriptors(CurrentFrame);
        Scene->FlushPendingUpdates(SCENE::SCENE_UPDATE_TYPE_ALL_PENDING, CurrentFrame);
    }

    //Resource copying passes
    for (uint32_t i = 0; i < RenderPasses.size(); i++)
    {
        RenderPasses[i].Scene->ResourceManagerPtr->QueueCopyOperations(MainCommandBuffer, CurrentFrame, FrameGraph);
    }

    bool EnableCulling = true;

    FrameGraph.AppendTask({
          [&](RENDERER::PassBuilder& Builder) {

              for (auto SceneCameraPair : UniqueSceneCameraPairs)
              {
                    auto& IndirectBuffer = SceneCameraPair.second.first->MeshBuffers.Buffers.IndirectBuffers[CurrentFrame].Buffer; //TODO wth
                    Builder.Write(&IndirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT);
              }
          },

          [&](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
             // std::cout << "Pass [ResetCullingPass_Internal] is being executed..." << std::endl;

              for (auto SceneCameraPair : UniqueSceneCameraPairs)
              {
                  DispatchComputeResetCulling(
                      *SceneCameraPair.second.first,
                      *SceneCameraPair.second.second,
                      MainCommandBuffer,
                      CurrentImageIndex,
                      CurrentFrame,
                      PipelineBarrier2,
                      EnableCulling
                  );
              }
          },

          "ResetCullingPass_Internal"
    });

    FrameGraph.AppendTask({
           [&](RENDERER::PassBuilder& Builder) {

              for (auto SceneCameraPair : UniqueSceneCameraPairs)
              {
                  auto& SceneBuffers = SceneCameraPair.second.first->MeshBuffers.Buffers;
                  auto& IndirectBuffer = SceneBuffers.IndirectBuffers[CurrentFrame].Buffer; 
                  auto& DrawMetaDataBuffer = SceneBuffers.DrawMetaDataBuffer[CurrentFrame].Buffer;
                  auto& ModelMatricesBuffer = SceneBuffers.ModelMatricesBuffers[CurrentFrame].Buffer;
                  auto& VisibilityIndexBuffer = SceneBuffers.CullBuffers.VisibilityIndexBuffers[CurrentFrame].Buffer;
                  Builder.Write(&IndirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
                  Builder.Write(&VisibilityIndexBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT);
                  Builder.Read(&DrawMetaDataBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
                  Builder.Read(&ModelMatricesBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
              }
          },

          [&](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
            //std::cout << "Pass [CullingPass_Internal] is being executed..." << std::endl;

            //Culling passes
            for (auto SceneCameraPair : UniqueSceneCameraPairs)
            {
                DispatchComputeCulling(
                    *SceneCameraPair.second.first,
                    *SceneCameraPair.second.second,
                    MainCommandBuffer,
                    CurrentImageIndex,
                    CurrentFrame,
                    PipelineBarrier2,
                    EnableCulling
                );
            }
          },

          "CullingPass_Internal"
    });

    //Rendering passes
    for (uint32_t i = 0; i < RenderPasses.size(); i++)
    {
        bool IsFirstOne = i == 0;
        RenderPassConfiguration& RenderPass = RenderPasses[i];

        //vkCmdSetViewport(MainCommandBuffer, 0, 1, &RenderPass.Viewport);
       // vkCmdSetScissor(MainCommandBuffer, 0, 1, &RenderPass.Scissor);

        RenderPass.Pipeline->QueueRenderTasks(
            FrameGraph,
            *RenderPass.Scene,
            *RenderPass.Camera,
            MainCommandBuffer,
            CurrentImageIndex,
            CurrentFrame,
            DepthImages[CurrentFrame],
            ColorRenderAttachmentImages[CurrentFrame],
            Gbuffers[CurrentFrame],
            LightingPassDescriptorSets[CurrentFrame],
            RenderPass.EnableDepthTesting,
            IsFirstOne,
            IsFirstOne
        );
    }

    FrameGraph.AppendTask({
          [&](RENDERER::PassBuilder& Builder) {
               
                Builder.Read(
                    &DepthImages[CurrentFrame], 
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, 
                    VK_IMAGE_ASPECT_DEPTH_BIT
                );
                Builder.Read(
                    &ColorRenderAttachmentImages[CurrentFrame],
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
                );
                Builder.Write(
                    &SwapChainImage,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                );
         },

         [&](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
            //std::cout << "Pass [PostProcessPass_Internal] is being executed..." << std::endl;

            //Post process pass that renders the final rendered image onto the swapchain image with effects 
            RenderPostProcessPass(SCENE::Camera3D(), MainCommandBuffer, CurrentImageIndex, CurrentFrame);
         },

         "PostProcessPass_Internal"
    });

    vkCmdSetViewport(MainCommandBuffer, 0, 1, &Viewport1);
    vkCmdSetScissor(MainCommandBuffer, 0, 1, &Scissor1);
 
    FrameGraph.Compile(CurrentFrame);
    FrameGraph.Execute(MainCommandBuffer, CurrentFrame);

    RENDERER_CORE::SafeImageBarrier(
        RendererContextPtr->SwapChain.SwapChainImages[CurrentImageIndex],
        SwapChainImage.BarrierState,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0
    );
    PipelineBarrier2.ExecutePipelineBarrier(MainCommandBuffer);
    SwapChainBarrierState = SwapChainImage.BarrierState;

    if (vkEndCommandBuffer(MainCommandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer");
    }

    uint64_t SignalValues[] = { 0,GlobalTimelineCounter };
    VkTimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo = RENDERER_CORE::TimelineSemaphoreSubmitInfo(nullptr, 0, SignalValues, 2);

    RENDERER_CORE::SubmitQueue(
        RendererContextPtr->DeviceContext.GraphicsQueue,
        { CurrentSyncObjects.ImageAvailableSemaphore },
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        { MainCommandBuffer },
        { CurrentSyncObjects.RenderFinishedSemaphore , TimelineSemaphore.Handle },
        nullptr,
        &TimelineSemaphoreSubmitInfo
    );

    VkResult Result = FrameManager.EndFrame(
        RendererContextPtr->DeviceContext.LogicalDevice,
        RendererContextPtr->DeviceContext.PresentQueue,
        RendererContextPtr->SwapChain.Handle,
        RendererContextPtr->Window
    );
    if (Result != VK_SUCCESS)
    {
        OnRecreateSwapChain();
        return;
    }
    CurrentFrame = FrameManager.CurrentFrame;
}

void RENDERER::Renderer::ExecuteTasksHeadless()
{

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
        RendererContextPtr->SwapChain.SwapChainImagesViews[CurrentImageIndex],
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

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.Handle);

    VkBuffer VertexBuffers[] = { PhysicsDebugLineVertexBuffers[CurrentFrame].BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    //vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PhysicsDebugGraphicsPipeline.Layout, 0, 1, &GbufferPassDescriptorSets[CurrentFrame], 0, nullptr);

    glm::mat4 Matrices[2] = { Camera.ViewMatrix , Camera.ProjectionMatrix };

    vkCmdPushConstants(
        CommandBuffer,
        PhysicsDebugGraphicsPipeline.Layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &Camera.ViewMatrix
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
        RendererContextPtr->SwapChain.SwapChainImagesViews[CurrentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    size_t CurrentPipelineIndex = RendererContextPtr->DefaultPipelines.PostProcessing;
    RENDERER_CORE::GraphicsPipelineEntry* CurrentPipelineEntry = RendererContextPtr->PipelineManager.GetGraphicsPipeline(CurrentPipelineIndex);
    assert(CurrentPipelineEntry);
    RENDERER_CORE::GraphicsPipeline CurrentPipeline = CurrentPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Handle);

    VkBuffer VertexBuffers[] = { RendererContextPtr->QuadVertexBuffer.BufferObject };
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

    RendererContextPtr->ShaderManager.AppendShaderModule(
        "TestComputeShader",
        "Shaders\\TestComputeShader.comp",
        "Shaders\\TestComputeShader.spv",
        shaderc_compute_shader,
        LogicalDevice
    );

    RENDERER_CORE::ComputePipelineCreateInfo Info{};
    Info.DescriptorSetLayouts = { TestDescriptor.Layout };
    Info.ComputeShaderModule = RendererContextPtr->ShaderManager.GetShaderModule("TestComputeShader");
    TestPipeline.Create(Info,LogicalDevice);

    size_t BufferSize = sizeof(int) * 5;

    RENDERER_CORE::CreateBuffer(
        PhysicalDevice, 
        LogicalDevice,
        BufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        TestBuffer
    );
    RENDERER_CORE::MapBuffer(TestBuffer, LogicalDevice, 0, BufferSize, 0);
    //TestBuffer.Map(LogicalDevice, 0, BufferSize, 0);

    RENDERER_CORE::DescriptorSetWriteBuffer BufferWrite{};
    BufferWrite.Create(TestBuffer, BufferSize, 0, TestDescriptor.DescriptorSets[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { BufferWrite }, {});
}

void RENDERER::Renderer::DestroyTestResources()
{
    TestPipeline.Destroy(LogicalDevice);
    TestDescriptor.Destroy(LogicalDevice);
    RendererContextPtr->ShaderManager.EraseShaderModule("TestComputeShader", LogicalDevice);
    RENDERER_CORE::DestroyBuffer(LogicalDevice, TestBuffer);
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
    uint32_t EnabledMeshCount = MeshBuffers.Buffers.EnabledMeshCount[CurrentFrame];
    uint32_t IndirectCommandCount = MeshBuffers.Entries[CurrentFrame].MeshEntries.size();
    if (!IndirectCommandCount || !EnabledMeshCount || !EnableCulling) return;
    //std::cout << "CurrentFrame: " << CurrentFrame << " EnabledMeshCount: " << EnabledMeshCount << " IndirectCommandCount: " << IndirectCommandCount << std::endl;
    int WorkGroupSize = 64;

    uint32_t CullingWorkGroupCount = (EnabledMeshCount + WorkGroupSize - 1) / WorkGroupSize;
    const size_t CullingPipelineIndex = RendererContextPtr->DefaultPipelines.CullingCompute;
    RENDERER_CORE::ComputePipelineEntry* CullingPipelineEntry = RendererContextPtr->PipelineManager.GetComputePipeline(CullingPipelineIndex);
    assert(CullingPipelineEntry);
    RENDERER_CORE::ComputePipeline& CullingPipeline = CullingPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingPipeline.Handle);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CullingPipeline.Layout, 0, 1, &Scene.IndirectDescriptorSets[CurrentFrame], 0, nullptr);

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

    vkCmdDispatch(CommandBuffer, CullingWorkGroupCount, 1, 1);

    /*
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.Buffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.Buffers.ModelMatricesBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.Buffers.DrawMetaDataBuffer[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.Buffers.CullBuffers.VisibilityIndexBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT
    );
    */
    //PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);
}

void RENDERER::Renderer::DispatchComputeResetCulling(SCENE::Scene& Scene, SCENE::Camera3D& Camera, VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame, RENDERER_CORE::PipelineBarrier2& PipelineBarrier2, bool EnableCulling)
{
    auto& MeshBuffers = Scene.MeshBuffers;
    uint32_t EnabledMeshCount = MeshBuffers.Buffers.EnabledMeshCount[CurrentFrame];
    uint32_t IndirectCommandCount = MeshBuffers.Entries[CurrentFrame].MeshEntries.size();
    if (!IndirectCommandCount || !EnabledMeshCount || !EnableCulling) return;
    int WorkGroupSize = 64;

    uint32_t CullingResetWorkGroupCount = (IndirectCommandCount + WorkGroupSize - 1) / WorkGroupSize;
    const size_t CullingResetPipelineIndex = RendererContextPtr->DefaultPipelines.CullingResetCompute;
    RENDERER_CORE::ComputePipelineEntry* CullingResetPipelineEntry = RendererContextPtr->PipelineManager.GetComputePipeline(CullingResetPipelineIndex);
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

    /*
    PipelineBarrier2.AppendBufferMemoryBarrier(
        MeshBuffers.Buffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
    );
    */
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
    PipelineCreateInfo.ViewportWidth = static_cast<float>(RendererContextPtr->SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(RendererContextPtr->SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { RendererContextPtr->SwapChain.Extent.width ,RendererContextPtr->SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R8G8B8A8_SRGB };
   // PipelineCreateInfo.ShaderModules = { {&LightingVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&LightingFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.AttributeDescriptions = RendererContextPtr->QuadVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = RendererContextPtr->QuadVertexDescription.BindingDescription;
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineCreateInfo.DescriptorSetLayouts = { RendererContextPtr->LightingPassLayout,RendererContextPtr->SceneDescriptorSetLayout };
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
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = RendererContextPtr->DepthImageFormat;
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
    glfwGetFramebufferSize(RendererContextPtr->Window.Handle, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(RendererContextPtr->Window.Handle, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(LogicalDevice);

    RendererContextPtr->SwapChain.Destroy(LogicalDevice);
    RendererContextPtr->SwapChain.Create(PhysicalDevice, LogicalDevice, RendererContextPtr->Surface.Handle, RendererContextPtr->Window.Handle);

    std::vector<RENDERER_CORE::DescriptorSetWriteImage> ImageWrites;
    ImageWrites.reserve(7 * MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        //Gbuffer recreation
        Gbuffers[i].Destroy(LogicalDevice);
        Gbuffers[i].Create(PhysicalDevice, LogicalDevice, RendererContextPtr->SwapChain.Extent.width, RendererContextPtr->SwapChain.Extent.height);

        //Depth image recreation
        DepthImages[i].Destroy(LogicalDevice);
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice, 
            RendererContextPtr->SwapChain.Extent.width, 
            RendererContextPtr->SwapChain.Extent.height, 
            VK_IMAGE_TILING_OPTIMAL, 
            RendererContextPtr->DepthImageFormat, 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            DepthImages[i].Image, 
            DepthImages[i].ImageMemory
        );
        DepthImages[i].ImageView = RENDERER_CORE::CreateImageView(DepthImages[i].Image, RendererContextPtr->DepthImageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, LogicalDevice);
        RENDERER_CORE::CreateTextureSampler(PhysicalDevice, LogicalDevice, DepthImages[i].Sampler, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        //Main render attachment recreation
        ColorRenderAttachmentImages[i].Destroy(LogicalDevice);
        RENDERER_CORE::CreateImage(
            PhysicalDevice,
            LogicalDevice,
            RendererContextPtr->SwapChain.Extent.width,
            RendererContextPtr->SwapChain.Extent.height,
            VK_IMAGE_TILING_OPTIMAL,
            RendererContextPtr->SwapChain.SurfaceFormat.format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ColorRenderAttachmentImages[i].Image,
            ColorRenderAttachmentImages[i].ImageMemory
        );
        ColorRenderAttachmentImages[i].ImageView = RENDERER_CORE::CreateImageView(
            ColorRenderAttachmentImages[i].Image,
            RendererContextPtr->SwapChain.SurfaceFormat.format,
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
        RenderPass.Pipeline->OnResize(RendererContextPtr->SwapChain.Extent.width, RendererContextPtr->SwapChain.Extent.height);
    }
}

void RENDERER::Renderer::Destroy()
{
    if (IsDestroyed) return;

    RENDERER_CORE::DestroyFrameSyncObjects(LogicalDevice, SyncObjects);
    FrameManager.Destroy(LogicalDevice);
    TimelineSemaphore.Destroy(LogicalDevice);

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
            RENDERER_CORE::DestroyBuffer(LogicalDevice, PhysicsDebugLineVertexBuffer);
        }
    }
    
    DestroyTestResources();

    PhysicsDebugGraphicsPipeline.Destroy(LogicalDevice);
    IsDestroyed = true;
    std::cout << "Renderer destroyed!" << std::endl;
};
