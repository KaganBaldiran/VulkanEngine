#include "DeferredRenderPipeline.hpp"
#include "GeometryBuffer.hpp"
#include "MeshManager.hpp"
#include "MaterialManager.hpp"
#include "ResourceManager.hpp"
#include "RendererContext.hpp"

#include "../Scene/Scene.hpp"
#include "../Scene/Camera.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include <algorithm>

RENDERER::DeferredRenderPipeline::DeferredRenderPipeline(RendererContext& RendererContext)
{
    Create(RendererContext);
}

void RENDERER::DeferredRenderPipeline::Create(RendererContext& RendererContext)
{
    this->RendererContextPtr = &RendererContext;

    LogicalDevice = RendererContext.DeviceContext.LogicalDevice;
    PhysicalDevice = RendererContext.DeviceContext.PhysicalDevice;
    GraphicsQueueIndex = RendererContext.QueueFamilyIndices.GraphicsFamily.value();

    HasCustomPipeline.fill(false);
    this->StartingTime = std::chrono::system_clock::now();

    PipelineType = RENDER_PIPELINE_TYPE_DEFERRED_RENDER;
    this->IsDestroyed = false;
    this->DestructionPriority = 1;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::DeferredRenderPipeline::CompileCustomPipeline(std::string ShadePixelFunction,const char* Label)
{
    if (ShadePixelFunction.empty()) throw std::runtime_error("Shader pixel function cannot be empty!");

    RENDERER_CORE::ShaderModule* FragmentShaderPtr = nullptr;
    if (!(FragmentShaderPtr = RendererContextPtr->ShaderManager.GetShaderModule(Label)))
    {
        std::vector<char> File = RENDERER_CORE::ReadFile("Shaders\\CustomizableDeferredShading.frag");
        std::string FileText(File.begin(), File.end());
        auto TargetLocation = FileText.find("//APPENDSPOT");
        if (TargetLocation == std::string::npos) return;

        FileText.insert(TargetLocation + 13, ShadePixelFunction);
        std::string SpirvFileName = Label + std::string(".spv");
        RENDERER_CORE::ShaderData ShaderData;
        if (VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS)
        {
            ShaderData.FromGLSL(FileText, shaderc_fragment_shader, Label);
            ShaderData.WriteFileSpirv(SpirvFileName.c_str());
        }
        else ShaderData.FromSpirV(SpirvFileName.c_str());
        FragmentShaderPtr = RendererContextPtr->ShaderManager.AppendShaderModule(Label, ShaderData, RendererContextPtr->DeviceContext.LogicalDevice);
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (HasCustomPipeline[i]) {
            RendererContextPtr->PipelineManager.EraseGraphicsPipelineByHash(PipelineIndices[i], RendererContextPtr->DeviceContext.LogicalDevice);
        }
        size_t DefaultPipelineIndex = RendererContextPtr->DefaultPipelines.DeferredShading[i];
        auto PipelineCreateInfo = RendererContextPtr->PipelineManager.GetGraphicsPipeline(DefaultPipelineIndex)->PipelineCreateInfo;
        PipelineCreateInfo.ShaderModules[1] = { FragmentShaderPtr , VK_SHADER_STAGE_FRAGMENT_BIT };
        auto PipelineIterator = RendererContextPtr->PipelineManager.AppendGraphicsPipeline(
            PipelineCreateInfo,
            RendererContextPtr->DeviceContext.LogicalDevice
        );
        //Update the custom pipeline map to let the context know of the change.
        PipelineIndices[i] = PipelineIterator.second;
        RendererContextPtr->CustomPipelines[i][this->GetHandleID()] = PipelineIterator.second;
        HasCustomPipeline[i] = true;
    }
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Created custom deferred pipeline (" + std::string(Label) + ")."));
}

void RENDERER::DeferredRenderPipeline::RenderScene(
    SCENE::Scene& Scene,
    SCENE::Camera3D& Camera,
    VkCommandBuffer& CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame,
    VkImageView& DepthImageImageView,
    VkImageView& DstColorRenderTargetImageViews,
    GeometryBuffer& FrameGbuffer,
    VkDescriptorSet& GeometrybufferDescriptorSet,
    bool EnableDepthTesting,
    bool ClearDepth,
    bool ClearColorAttachment
)
{
    if (!Scene.MeshBuffers.Buffers.EnabledMeshCount[CurrentFrame] || !Scene.ResourceManagerPtr)
    {
        return;
    };

    std::array<RENDERER_CORE::ImageData*, 4> GbufferAttachments = {
        &FrameGbuffer.NormalAttachment,
        &FrameGbuffer.PositionAttachment,
        &FrameGbuffer.AlbedoAttachment,
        &FrameGbuffer.RoughnessMetallicAttachment
    };

    for (auto& Attachment : GbufferAttachments)
    {
        RENDERER_CORE::SafeImageBarrier(
            Attachment->Image,
            Attachment->BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        );
    }

    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);
    RenderGeometryPass(
        Scene, 
        Camera,
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame, 
        DepthImageImageView,
        FrameGbuffer,
        EnableDepthTesting,
        ClearDepth
    );

    for (auto& Attachment : GbufferAttachments)
    {
        RENDERER_CORE::SafeImageBarrier(
            Attachment->Image,
            Attachment->BarrierState,
            PipelineBarrier2,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );
    }
    PipelineBarrier2.ExecutePipelineBarrier(CommandBuffer);

    RenderLightingPass(
        Scene, 
        Camera,
        CommandBuffer, 
        CurrentImageIndex, 
        CurrentFrame,
        DstColorRenderTargetImageViews,
        GeometrybufferDescriptorSet,
        ClearColorAttachment
    );
}

void RENDERER::DeferredRenderPipeline::QueueRenderTasks(
    FrameGraph& FrameGraph, 
    SCENE::Scene& Scene, 
    SCENE::Camera3D& Camera, 
    VkCommandBuffer CommandBuffer, 
    uint32_t CurrentImageIndex, 
    uint32_t CurrentFrame, 
    RENDERER_CORE::ImageData& DepthImageImage,
    RENDERER_CORE::ImageData& DstColorRenderTargetImage, 
    GeometryBuffer& FrameGbuffer, 
    VkDescriptorSet GeometrybufferDescriptorSet, 
    bool EnableDepthTesting, 
    bool ClearDepth, 
    bool ClearColorAttachment
)
{
    if (!Scene.MeshBuffers.Buffers.EnabledMeshCount[CurrentFrame] || !Scene.ResourceManagerPtr)
    {
        return;
    };

    FrameGraph.AppendTask({

        [&,CurrentFrame](RENDERER::PassBuilder& Builder) {
            std::array<RENDERER_CORE::ImageData*, 4> GbufferAttachments = {
                &FrameGbuffer.NormalAttachment,
                &FrameGbuffer.PositionAttachment,
                &FrameGbuffer.AlbedoAttachment,
                &FrameGbuffer.RoughnessMetallicAttachment
            };

            for (auto& Attachment : GbufferAttachments)
            {
                Builder.Write(
                    Attachment,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                );
            }

            Builder.Write(
                &DepthImageImage,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );

            auto& SceneBuffers = Scene.MeshBuffers.Buffers;
            auto& IndirectBuffer = SceneBuffers.IndirectBuffers[CurrentFrame].Buffer;
            auto& DrawMetaDataBuffer = SceneBuffers.DrawMetaDataBuffer[CurrentFrame].Buffer;
            auto& ModelMatricesBuffer = SceneBuffers.ModelMatricesBuffers[CurrentFrame].Buffer;
            auto& VisibilityIndexBuffer = SceneBuffers.CullBuffers.VisibilityIndexBuffers[CurrentFrame].Buffer;
            auto& TexturesIndexBuffer = SceneBuffers.TexturesIndexBuffers[CurrentFrame].Buffer;
            Builder.Read(&IndirectBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
            Builder.Read(&VisibilityIndexBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
            Builder.Read(&DrawMetaDataBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
            Builder.Read(&ModelMatricesBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
            Builder.Read(&TexturesIndexBuffer, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
        },

        [&Scene, &Camera, CurrentImageIndex, &DepthImageImage, 
        &FrameGbuffer, EnableDepthTesting, ClearDepth, this](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
           // std::cout << "Pass [GbufferPass_Internal] is being executed..." << std::endl;

            RenderGeometryPass(
                Scene,
                Camera,
                CommandBuffer,
                CurrentImageIndex,
                CurrentFrame,
                DepthImageImage.ImageView,
                FrameGbuffer,
                EnableDepthTesting,
                ClearDepth
            );
        },

        "GbufferPass_Internal"
    });

    FrameGraph.AppendTask({

         [&,CurrentFrame](RENDERER::PassBuilder& Builder) {
            std::array<RENDERER_CORE::ImageData*, 4> GbufferAttachments = {
                &FrameGbuffer.NormalAttachment,
                &FrameGbuffer.PositionAttachment,
                &FrameGbuffer.AlbedoAttachment,
                &FrameGbuffer.RoughnessMetallicAttachment
            };

            for (auto& Attachment : GbufferAttachments)
            {
                Builder.Read(
                    Attachment,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT
                );
            }

            auto& SceneBuffers = Scene.MeshBuffers.Buffers;
            auto& SceneLightManager = Scene.LightManager;

            auto& StaticLightBuffer = SceneLightManager.StaticLightSSBOs[CurrentFrame].Buffer;
            auto& DynamicLightBuffer = SceneLightManager.DynamicLightSSBOs[CurrentFrame].Buffer;
            auto& TexturesIndexBuffer = SceneBuffers.TexturesIndexBuffers[CurrentFrame].Buffer;
            Builder.Read(&StaticLightBuffer, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
            Builder.Read(&DynamicLightBuffer, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
            Builder.Read(&TexturesIndexBuffer, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_MEMORY_READ_BIT);

            Builder.Write(
                &DstColorRenderTargetImage,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
            );
         },

        [&Scene, &Camera, CurrentImageIndex, CurrentFrame, 
        &DstColorRenderTargetImage, GeometrybufferDescriptorSet, 
        ClearColorAttachment, this](VkCommandBuffer CommandBuffer,uint32_t FrameIndex) {
            //std::cout << "Pass [LightingPass_Internal] is being executed..." << std::endl;
           
            RenderLightingPass(
                Scene,
                Camera,
                CommandBuffer,
                CurrentImageIndex,
                FrameIndex,
                DstColorRenderTargetImage.ImageView,
                GeometrybufferDescriptorSet,
                ClearColorAttachment
            );
        },

        "LightingPass_Internal"
    });
}

void RENDERER::DeferredRenderPipeline::Destroy()
{
    if (IsDestroyed) return;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (!HasCustomPipeline[i]) continue;
        RendererContextPtr->CustomPipelines[i].erase(this->GetHandleID());
        RendererContextPtr->PipelineManager.EraseGraphicsPipelineByHash(PipelineIndices[i], RendererContextPtr->DeviceContext.LogicalDevice);
        HasCustomPipeline[i] = false;
    }
    IsDestroyed = true;
}

void RENDERER::DeferredRenderPipeline::OnResize(uint32_t Width, uint32_t Height)
{
    
}

void RENDERER::DeferredRenderPipeline::RenderGeometryPass(
    SCENE::Scene& Scene,
    SCENE::Camera3D& Camera,
    VkCommandBuffer CommandBuffer,
    uint32_t CurrentImageIndex,
    uint32_t CurrentFrame,
    VkImageView DepthImage,
    GeometryBuffer &Gbuffers,
    bool EnableDepthTesting,
    bool ClearDepth
)
{
    const size_t PerformanceModeEnabledMeshCount = Scene.MeshBuffers.Entries[CurrentFrame].MeshEntries.size();
    if (!(PerformanceModeEnabledMeshCount)) return;

    std::array<VkClearValue, 4> ClearColors{};
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
    RENDERER::MeshManager& MeshManager = Scene.ResourceManagerPtr->MeshManager;

    const size_t CurrentPipelineIndex = EnableDepthTesting ? RendererContextPtr->DefaultPipelines.GbufferDepthEnabled[CurrentFrame] :
                                                              RendererContextPtr->DefaultPipelines.GbufferDepthDisabled[CurrentFrame];
    RENDERER_CORE::GraphicsPipelineEntry* CurrentPipelineEntry = RendererContextPtr->PipelineManager.GetGraphicsPipeline(CurrentPipelineIndex);
    assert(CurrentPipelineEntry);
    RENDERER_CORE::GraphicsPipeline& CurrentPipeline = CurrentPipelineEntry->Pipeline;

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Handle);
    VkDescriptorSet DescriptorSets[] = { 
        Scene.IndirectDescriptorSets[CurrentFrame],
        RendererContextPtr->TexturesDescriptors[CurrentFrame].DescriptorSets[0],
        Scene.TextureIndicesDescriptorSets[CurrentFrame] 
    };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 3, DescriptorSets, 0, nullptr);

   /* VkBuffer VertexBuffers[] = { MeshManager.GetCurrentVertexBuffer(CurrentFrame).Buffer.BufferObject };
    VkDeviceSize VertexOffsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, VertexOffsets);
    VkDeviceSize IndexOffset = 0;
    vkCmdBindIndexBuffer(CommandBuffer, MeshManager.GetCurrentIndexBuffer(CurrentFrame).Buffer.BufferObject, IndexOffset, VK_INDEX_TYPE_UINT32);*/

    glm::mat4 ProjViewMatrix;

    if (IsCamera1)
    {
        ProjViewMatrix = Camera.ProjectionMatrix * Camera.ViewMatrix;
    }
    else
    {
        glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
        float FOV = 45.0f;
        glm::ivec2 Extent = { RendererContextPtr->SwapChain.Extent.width,RendererContextPtr->SwapChain.Extent.height };
        float Near = 0.1f;
        float Far = 2000.0f;

        glm::mat4 ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraDirection, Up);
        glm::mat4 ProjectionMatrix = glm::perspective(glm::radians((float)FOV), (float)Extent.x / (float)Extent.y, Near, Far);
        ProjectionMatrix[1][1] *= -1;

        ProjViewMatrix = ProjectionMatrix * ViewMatrix;
    }
    //glm::mat4 ProjViewMatrix = Scene.Camera->ProjectionMatrix * Scene.Camera->ViewMatrix;
    
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

    auto& PageMeshCounts = Scene.MeshBuffers.PageMeshCounts[CurrentFrame];
    size_t Offset = 0;
    for (size_t i = 0; i < PageMeshCounts.size(); i++)
    {
        auto& PageMeshCount = PageMeshCounts[i];

        VkBuffer GeometryBuffers[] = {MeshManager.GeometryBufferPages[i].Buffer.BufferObject};
        VkDeviceSize VertexOffsets[] = { 0 };
        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, GeometryBuffers, VertexOffsets);
        VkDeviceSize IndexOffset = 0;
        vkCmdBindIndexBuffer(CommandBuffer, MeshManager.GeometryBufferPages[i].Buffer.BufferObject, IndexOffset, VK_INDEX_TYPE_UINT32);

        if (RendererContextPtr->DeviceContext.DeviceFeatures2.features.multiDrawIndirect)
        {
            vkCmdDrawIndexedIndirect(
                CommandBuffer,
                Scene.MeshBuffers.Buffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
                Offset,
                PageMeshCount.MeshCount,
                sizeof(SCENE::ExtendedIndirectCommand)
            );
        }
        else
        {
            for (size_t j = 0; j < PageMeshCount.MeshCount; j++)
            {
                vkCmdDrawIndexedIndirect(
                    CommandBuffer,
                    Scene.MeshBuffers.Buffers.IndirectBuffers[CurrentFrame].Buffer.BufferObject,
                    Offset + j * sizeof(SCENE::ExtendedIndirectCommand),
                    1,
                    sizeof(SCENE::ExtendedIndirectCommand)
                );
            }
        }
        Offset += PageMeshCount.MeshCount * sizeof(SCENE::ExtendedIndirectCommand);
    }

    RenderingPass.EndRendering(CommandBuffer);
}

void RENDERER::DeferredRenderPipeline::RenderLightingPass(
    SCENE::Scene& Scene, 
    SCENE::Camera3D& Camera,
    VkCommandBuffer CommandBuffer,
    uint32_t CurrentImageIndex, 
    uint32_t CurrentFrame, 
    VkImageView DstRenderTargetImageView,
    VkDescriptorSet GeometrybufferDescriptorSet,
    bool ClearColorAttachment
)
{
    std::array<VkClearValue, 1> ClearColors{};
    ClearColors[0].color = { {0.0f,0.0f,0.0f,1.0f} };

    RENDERER_CORE::DynamicRenderingPass RenderingPass;
    RenderingPass.AppendAttachment(
        DstRenderTargetImageView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        ClearColorAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        ClearColors[0]
    );

    RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)RendererContextPtr->SwapChain.Extent.width, (uint32_t)RendererContextPtr->SwapChain.Extent.height} });

    size_t CurrentPipelineIndex = HasCustomPipeline[CurrentFrame] ? PipelineIndices[CurrentFrame] : RendererContextPtr->DefaultPipelines.DeferredShading[CurrentFrame];
    RENDERER_CORE::GraphicsPipelineEntry* CurrentPipelineEntry = RendererContextPtr->PipelineManager.GetGraphicsPipeline(CurrentPipelineIndex);
    RENDERER_CORE::GraphicsPipeline& CurrentPipeline = CurrentPipelineEntry->Pipeline;
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Handle);

    VkBuffer VertexBuffers[] = { RendererContextPtr->QuadVertexBuffer.BufferObject };
    VkDeviceSize Offsets[] = { 0 };
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);
    VkDescriptorSet DescriptorSets[] = { 
        GeometrybufferDescriptorSet,
        Scene.SceneDescriptorSets[CurrentFrame] ,
        Scene.TextureIndicesDescriptorSets[CurrentFrame], 
        RendererContextPtr->TexturesDescriptors[CurrentFrame].DescriptorSets[0]
    };
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CurrentPipeline.Layout, 0, 4, DescriptorSets, 0, nullptr);
    
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    float FOV = 45.0f;
    glm::ivec2 Extent = { RendererContextPtr->SwapChain.Extent.width,RendererContextPtr->SwapChain.Extent.height };
    float Near = 0.1f;
    float Far = 2000.0f;

    LightingPassUBOdata lightingPassUboData{};
    //lightingPassUboData.CameraDirection = Scene.Camera->CameraDirection;
    //lightingPassUboData.CameraPosition = Scene.Camera->CameraPosition;

    if (IsCamera1)
    {
       lightingPassUboData.CameraDirection = Camera.CameraDirection;
       lightingPassUboData.CameraPosition = Camera.CameraPosition;
    }
    else
    {
       lightingPassUboData.CameraPosition = CameraPosition;
       lightingPassUboData.CameraDirection = CameraDirection;
    }

    lightingPassUboData.FogIntensity = 0.4f;
    lightingPassUboData.StaticLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].StaticLightLights.size());
    lightingPassUboData.DynamicLightCount = static_cast<int>(Scene.LightManager.LightEntries[CurrentFrame].DynamicLights.size());
    //lightingPassUboData.CameraFrustumLength = Scene.Camera->FarPlane - Scene.Camera->NearPlane;
    lightingPassUboData.CameraFrustumLength = Far - Near;
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
