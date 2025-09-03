#include "DeferredRenderPipeline.hpp"
#include "../Scene/Scene.hpp"
#include "../Scene/MeshManager.hpp"
#include "../Scene/MaterialManager.hpp"
#include "../Scene/Camera.hpp"

RENDERER::DeferredRenderPipeline::DeferredRenderPipeline(RendererContext& RendererContext)
{
    Create(RendererContext);
}

void RENDERER::DeferredRenderPipeline::Create(RendererContext& RendererContext)
{
    this->RendererContextPtr = &RendererContext;

    LogicalDevice = RendererContext.DeviceContext.logicalDevice;
    PhysicalDevice = RendererContext.DeviceContext.physicalDevice;
    GraphicsQueueIndex = RendererContext.QueueFamilyIndices.GraphicsFamily.value();

    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Create(PhysicalDevice, LogicalDevice, RendererContext.SwapChain.Extent.width, RendererContext.SwapChain.Extent.height);
    }

    //Lighting pass descriptor set
    DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4 * MAX_FRAMES_IN_FLIGHT} },
        MAX_FRAMES_IN_FLIGHT, LogicalDevice
    );

    RENDERER_CORE::AllocateDescriptorSets(LogicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.descriptorPool, RendererContextPtr->LightingPassLayout.descriptorSetLayout, LightingPassDescriptorSets.data());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage NormalTextureWrite(Gbuffers[i].NormalAttachment.ImageView, Gbuffers[i].NormalAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage PositionTextureWrite(Gbuffers[i].PositionAttachment.ImageView, Gbuffers[i].PositionAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage AlbedoTextureWrite(Gbuffers[i].AlbedoAttachment.ImageView, Gbuffers[i].AlbedoAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::DescriptorSetWriteImage RoughnessMetallicTextureWrite(Gbuffers[i].RoughnessMetallicAttachment.ImageView, Gbuffers[i].RoughnessMetallicAttachment.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, LightingPassDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(LogicalDevice, { }, { NormalTextureWrite,PositionTextureWrite,AlbedoTextureWrite ,RoughnessMetallicTextureWrite });
    }

    this->IsDestroyed = false;
    this->DestructionPriority = 1;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::DeferredRenderPipeline::RenderScene(
    SCENE::Scene& Scene, 
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame, 
    VkImageView& DepthImageImageView, 
    VkImageView& DstRenderTargetImageView,
    bool EnableDepthTesting,
    bool ClearDepth,
    bool ClearColorAttachment
)
{
    if (!Scene.MeshBuffers.SceneBuffers.EnabledMeshCount[CurrentFrame] || !Scene.TextureManager->CurrentGbufferPassPipeline || !Scene.MeshManagerPtr)
    {
        return;
    };

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].NormalAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].PositionAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].AlbedoAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].RoughnessMetallicAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);
    RenderGeometryPass(
        Scene, 
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame, 
        DepthImageImageView,
        EnableDepthTesting,
        ClearDepth
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].NormalAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].PositionAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].AlbedoAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    RENDERER_CORE::SafeImageBarrier(
        Gbuffers[CurrentFrame].RoughnessMetallicAttachment,
        PipelineBarrier2,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

    RenderLightingPass(
        Scene, 
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame,
        DstRenderTargetImageView,
        ClearColorAttachment
    );
}

void RENDERER::DeferredRenderPipeline::Destroy()
{
    if (IsDestroyed) return;
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
    }
    DescriptorPool.Destroy(LogicalDevice);
    IsDestroyed = true;
}

void RENDERER::DeferredRenderPipeline::OnResize(uint32_t Width, uint32_t Height)
{
    for (auto& Gbuffer : Gbuffers)
    {
        Gbuffer.Destroy(LogicalDevice);
        Gbuffer.Create(PhysicalDevice, LogicalDevice, RendererContextPtr->SwapChain.Extent.width, RendererContextPtr->SwapChain.Extent.height);
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

void RENDERER::DeferredRenderPipeline::RenderGeometryPass(
    SCENE::Scene& Scene, 
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame,
    VkImageView& DepthImage,
    bool EnableDepthTesting,
    bool ClearDepth
)
{
    const size_t PerformanceModeEnabledMeshCount = Scene.MeshBuffers.SceneBuffers.EnabledMeshCount[CurrentFrame];
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
        DepthImage,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        ClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[2]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    RENDERER_CORE::GraphicsPipeline& CurrentPipeline = Scene.TextureManager->CurrentGbufferPassPipeline->at(static_cast<int>(EnableDepthTesting));
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.pipeline);
    VkDescriptorSet DescriptorSets[] = { Scene.IndirectDescriptorSets[CurrentFrame],Scene.TextureManager->TexturesDescriptors[CurrentFrame].DescriptorSets[0],
        Scene.TextureIndicesDescriptorSets[CurrentFrame] };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 3, DescriptorSets, 0, nullptr);

    VkBuffer VertexBuffers[] = { Scene.MeshManagerPtr->GetCurrentVertexBuffer(CurrentFrame).Buffer.BufferObject };
    VkDeviceSize VertexOffsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, VertexOffsets);
    VkDeviceSize IndexOffset = 0;
    vkCmdBindIndexBuffer(CommandBuffer, Scene.MeshManagerPtr->GetCurrentIndexBuffer(CurrentFrame).Buffer.BufferObject, IndexOffset, VK_INDEX_TYPE_UINT32);

    glm::mat4 Matrices[2] = { Scene.Camera->ViewMatrix , Scene.Camera->ProjectionMatrix };

    vkCmdPushConstants(
        CommandBuffer,
        CurrentPipeline.Layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4) * 2,
        &Matrices
    );

    if (RendererContextPtr->DeviceContext.DeviceFeatures.multiDrawIndirect)
    {
        vkCmdDrawIndexedIndirect(
            CommandBuffer,
            Scene.MeshBuffers.SceneBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
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
                Scene.MeshBuffers.SceneBuffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
                i * sizeof(SCENE::ExtendedIndirectCommand),
                1,
                sizeof(SCENE::ExtendedIndirectCommand)
            );
        }
    }

    RenderingPass.EndRendering(CommandBuffer);
}

void RENDERER::DeferredRenderPipeline::RenderLightingPass(
    SCENE::Scene& Scene, 
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex, 
    uint32_t CurrentFrame, 
    VkImageView& DstRenderTargetImageView,
    bool ClearColorAttachment
)
{
    std::array<VkClearValue, 2> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };
    ClearColors[1].depthStencil = { 1.0f,0 };

    RENDERER_CORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        DstRenderTargetImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        ClearColorAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContextPtr->DeferredLightingPassGraphicsPipeline.pipeline);

    VkBuffer VertexBuffers[] = { RendererContextPtr->QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = { LightingPassDescriptorSets[CurrentFrame],Scene.SceneDescriptorSets[CurrentFrame] };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContextPtr->DeferredLightingPassGraphicsPipeline.Layout, 0, 2, DescriptorSets, 0, nullptr);

    LightingPassUBOdata lightingPassUboData{};
    lightingPassUboData.CameraDirection = Scene.Camera->CameraDirection;
    lightingPassUboData.CameraPosition = Scene.Camera->CameraPosition;
    lightingPassUboData.FogIntensity = 0.4f;
    lightingPassUboData.StaticLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].StaticLightLights.size());
    lightingPassUboData.DynamicLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].DynamicLights.size());
    lightingPassUboData.CameraFrustumLength = Scene.Camera->FarPlane - Scene.Camera->NearPlane;

    vkCmdPushConstants(
        CommandBuffer,
        RendererContextPtr->DeferredLightingPassGraphicsPipeline.Layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(LightingPassUBOdata),
        &lightingPassUboData
    );

    vkCmdDraw(CommandBuffer, 4, 1, 0, 0);

    RenderingPass.EndRendering(CommandBuffer);
}
