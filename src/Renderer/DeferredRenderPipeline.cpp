#include "DeferredRenderPipeline.hpp"
#include "GeometryBuffer.hpp"

#include "../Scene/Scene.hpp"
#include "../Scene/MeshManager.hpp"
#include "../Scene/MaterialManager.hpp"
#include "../Scene/Camera.hpp"

#include <algorithm>

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

    this->StartingTime = std::chrono::system_clock::now();

    PipelineType = RENDER_PIPELINE_TYPE_DEFERRED_RENDER;
    this->IsDestroyed = false;
    this->DestructionPriority = 1;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::DeferredRenderPipeline::CompileCustomPipeline(std::string ShadePixelFunction,const char* Label)
{
    if (ShadePixelFunction.empty()) throw std::runtime_error("Shader pixel function cannot be empty!");
    static uint32_t CustomDeferredPipelineIterator = 0;

    std::vector<char> File = RENDERER_CORE::ReadFile("Shaders\\CustomizableDeferredShading.frag");
    std::string FileText(File.begin(),File.end());
    auto TargetLocation = FileText.find("//APPENDSPOT");
    if (TargetLocation == std::string::npos) return;
    
    FileText.insert(TargetLocation + 13, ShadePixelFunction);
    auto PipelineCreateInfo = RendererContextPtr->LatestShadingPassPipelineCreateInfo;

    std::string SpirvFileName = Label + std::string(".spv");
    RENDERER_CORE::ShaderModule FragmentShader;
    RENDERER_CORE::ShaderData ShaderData;
    if (VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS)
    {
        ShaderData.FromGLSL(FileText, shaderc_fragment_shader, Label);
        ShaderData.WriteFileSpirv(SpirvFileName.c_str());
    }
    else ShaderData.FromSpirV(SpirvFileName.c_str());
    FragmentShader.Create(ShaderData, LogicalDevice);

    PipelineCreateInfo.ShaderModules[1] = { &FragmentShader , VK_SHADER_STAGE_FRAGMENT_BIT };
    Pipeline.Destroy(LogicalDevice);
    Pipeline.Create(PipelineCreateInfo, LogicalDevice);
    FragmentShader.Destroy(RendererContextPtr->DeviceContext.logicalDevice);
}

void RENDERER::DeferredRenderPipeline::RenderScene(
    SCENE::Scene& Scene, 
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame, 
    VkImageView& DepthImageImageView, 
    VkImageView &DstColorRenderTargetImageViews,
    GeometryBuffer& FrameGbuffer,
    VkDescriptorSet& GeometrybufferDescriptorSet,
    bool EnableDepthTesting,
    bool ClearDepth,
    bool ClearColorAttachment
)
{
    if (!Scene.MeshBuffers.SceneBuffers.EnabledMeshCount[CurrentFrame] || !Scene.TextureManager->TextureDescriptorsPipelines || !Scene.MeshManagerPtr)
    {
        return;
    };

    for (auto& Attachment : { FrameGbuffer.NormalAttachment , FrameGbuffer.PositionAttachment , FrameGbuffer.AlbedoAttachment ,  FrameGbuffer.RoughnessMetallicAttachment })
    {
        RENDERER_CORE::SafeImageBarrier(
            const_cast<RENDERER_CORE::TextureData&>(Attachment),
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        );
    }

    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);
    RenderGeometryPass(
        Scene, 
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame, 
        DepthImageImageView,
        FrameGbuffer,
        EnableDepthTesting,
        ClearDepth
    );

    for (auto& Attachment : { FrameGbuffer.NormalAttachment , FrameGbuffer.PositionAttachment , FrameGbuffer.AlbedoAttachment ,  FrameGbuffer.RoughnessMetallicAttachment })
    {
        RENDERER_CORE::SafeImageBarrier(
            const_cast<RENDERER_CORE::TextureData&>(Attachment),
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );
    }
    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

    RenderLightingPass(
        Scene, 
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame,
        DstColorRenderTargetImageViews,
        GeometrybufferDescriptorSet,
        ClearColorAttachment
    );
}

void RENDERER::DeferredRenderPipeline::Destroy()
{
    if (IsDestroyed) return;
    
    IsDestroyed = true;
}

void RENDERER::DeferredRenderPipeline::OnResize(uint32_t Width, uint32_t Height)
{
    
}

void RENDERER::DeferredRenderPipeline::RenderGeometryPass(
    SCENE::Scene& Scene, 
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame,
    VkImageView& DepthImage,
    GeometryBuffer &Gbuffers,
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
    ClearColors[3].color = { {-1,0,0,0} };

    VkAttachmentLoadOp AttachmentLoadOp = ClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    RENDERER_CORE::DynamicRenderingPass RenderingPass;
   
    RenderingPass.AppendAttachment(
        Gbuffers.PositionAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        AttachmentLoadOp,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );
    
    RenderingPass.AppendAttachment(
        Gbuffers.NormalAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        AttachmentLoadOp,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );
   
    RenderingPass.AppendAttachment(
        Gbuffers.AlbedoAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[3]
    );

    RenderingPass.AppendAttachment(
        Gbuffers.RoughnessMetallicAttachment.ImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        AttachmentLoadOp,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[1]
    );

    RenderingPass.AppendAttachment(
        DepthImage,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        AttachmentLoadOp,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[2]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    RENDERER_CORE::GraphicsPipeline& CurrentPipeline = Scene.TextureManager->TextureDescriptorsPipelines->at(static_cast<int>(EnableDepthTesting));
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.pipeline);
    VkDescriptorSet DescriptorSets[] = { 
        Scene.IndirectDescriptorSets[CurrentFrame],
        Scene.TextureManager->TexturesDescriptors[CurrentFrame].DescriptorSets[0],
        Scene.TextureIndicesDescriptorSets[CurrentFrame] 
    };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 3, DescriptorSets, 0, nullptr);

    VkBuffer VertexBuffers[] = { Scene.MeshManagerPtr->GetCurrentVertexBuffer(CurrentFrame).Buffer.BufferObject };
    VkDeviceSize VertexOffsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, VertexOffsets);
    VkDeviceSize IndexOffset = 0;
    vkCmdBindIndexBuffer(CommandBuffer, Scene.MeshManagerPtr->GetCurrentIndexBuffer(CurrentFrame).Buffer.BufferObject, IndexOffset, VK_INDEX_TYPE_UINT32);

    glm::mat4 ProjViewMatrix = Scene.Camera->ProjectionMatrix * Scene.Camera->ViewMatrix;
    //glm::mat4 Matrices[2] = { Scene.Camera->ViewMatrix , Scene.Camera->ProjectionMatrix };
    glm::mat4 Matrices[2] = { ProjViewMatrix , PreviousProjViewMatrix };
    PreviousProjViewMatrix = ProjViewMatrix;

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
    VkDescriptorSet &GeometrybufferDescriptorSet,
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

    RENDERER_CORE::GraphicsPipeline& CurrentPipeline = Pipeline.pipeline ? Pipeline : Scene.TextureManager->TextureDescriptorsPipelines->at(2);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.pipeline);

    VkBuffer VertexBuffers[] = { RendererContextPtr->QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = { 
        GeometrybufferDescriptorSet,
        Scene.SceneDescriptorSets[CurrentFrame] ,
        Scene.TextureIndicesDescriptorSets[CurrentFrame], 
        Scene.TextureManager->TexturesDescriptors[CurrentFrame].DescriptorSets[0]
    };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 4, DescriptorSets, 0, nullptr);
    
    LightingPassUBOdata lightingPassUboData{};
    lightingPassUboData.CameraDirection = Scene.Camera->CameraDirection;
    lightingPassUboData.CameraPosition = Scene.Camera->CameraPosition;
    lightingPassUboData.FogIntensity = 0.4f;
    lightingPassUboData.StaticLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].StaticLightLights.size());
    lightingPassUboData.DynamicLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].DynamicLights.size());
    lightingPassUboData.CameraFrustumLength = Scene.Camera->FarPlane - Scene.Camera->NearPlane;
    lightingPassUboData.Time = std::chrono::duration<float>(std::chrono::system_clock::now() - StartingTime).count();

    vkCmdPushConstants(
        CommandBuffer,
        CurrentPipeline.Layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(LightingPassUBOdata),
        &lightingPassUboData
    );

    vkCmdDraw(CommandBuffer, 4, 1, 0, 0);

    RenderingPass.EndRendering(CommandBuffer);
}
