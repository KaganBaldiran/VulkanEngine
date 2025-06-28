#include "Scene.hpp"
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../app/RendererContext.hpp"
#include "Cubemap.hpp"

#include "DependencyManager.hpp"

VKSCENE::Scene::Scene(VKAPP::RendererContext& RendererContext)
{
    Create(RendererContext);
}

void VKSCENE::Scene::Create(VKAPP::RendererContext& RendererContext)
{
    //Light SSBO descriptor set
    DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3 * MAX_FRAMES_IN_FLIGHT},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MAX_FRAMES_IN_FLIGHT} },
        2 * MAX_FRAMES_IN_FLIGHT, RendererContext.DeviceContext.logicalDevice
    );
    
    SceneModelMatricesBuffer.resize(MAX_FRAMES_IN_FLIGHT);
    SceneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.descriptorPool, RendererContext.SceneDescriptorSetLayouts, SceneDescriptorSets);
    
    IndirectDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.descriptorPool, RendererContext.IndirectDescriptorSetLayouts, IndirectDescriptorSets);
    
    this->RendererContext = &RendererContext;
    DrawCubeMap = true;
}

void VKSCENE::Scene::Destroy()
{
    DescriptorPool.Destroy(RendererContext->DeviceContext.logicalDevice);
    DestroyMeshBuffers();
    DestroyLightBuffers();
}

void VKSCENE::Scene::SetCubemap(Cubemap& DestinationCubeMap)
{
    SceneCubeMap = &DestinationCubeMap;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteImage CubemapTextureWrite(SceneCubeMap->ConvolutionSampleImageView, SceneCubeMap->ConvolutionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, {}, { CubemapTextureWrite });
    }
}

void VKSCENE::Scene::SetCamera(Camera3D& Camera)
{
    this->Camera = &Camera;
}

void VKSCENE::Scene::UpdateDynamicLightBuffers()
{
    if (!this->DependencyManager) return;

    auto& DirtyResourceFlags = this->DependencyManager->DirtyResourceFlags;

    VkDeviceSize DynamicBufferSize = sizeof(LightData) * DynamicLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[0].MappedMemory);

    bool IsAnyUpdated = false;
    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DirtyResourceFlags[Light->ResourceID])
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
            IsAnyUpdated = true;
        }
        i++;
    }
    if (IsAnyUpdated)
    {
        for (size_t i = 1; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            memcpy(DynamicLightSSBO[i].MappedMemory, Destination, DynamicBufferSize);
        }
    }
}

void VKSCENE::Scene::UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame)
{
    if (!this->DependencyManager) return;

    auto& DirtyResourceFlags = this->DependencyManager->DirtyResourceFlags;
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[CurrentFrame].MappedMemory);

    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DirtyResourceFlags[Light->ResourceID])
        {
            Destination[i] = Light->Data;
        }
        i++;
    }
}

void VKSCENE::Scene::UpdateStaticLightBuffers()
{
    if (!this->DependencyManager) return;

    auto& DirtyResourceFlags = this->DependencyManager->DirtyResourceFlags;

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DirtyResourceFlags[Light->ResourceID])
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
        }
        i++;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CopyBuffer(
            StaticLightStagingBuffer.Buffer.BufferObject,
            StaticLightSSBO[i].BufferObject,
            StaticBufferSize,
            RendererContext->DeviceContext.logicalDevice,
            RendererContext->CommandPool.commandPool,
            RendererContext->DeviceContext.GraphicsQueue
        );
    }
}

void VKSCENE::Scene::UpdateStaticFrameLightBuffers(uint32_t CurrentFrame)
{
    if (!this->DependencyManager) return;

    auto& DirtyResourceFlags = this->DependencyManager->DirtyResourceFlags;

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DirtyResourceFlags[Light->ResourceID])
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
        }
        i++;
    }
    VKCORE::CopyBuffer(
        StaticLightStagingBuffer.Buffer.BufferObject,
        StaticLightSSBO[CurrentFrame].BufferObject,
        StaticBufferSize,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );
}

void VKSCENE::Scene::CreateMeshBuffers()
{
    if (Entities.empty()) return;

    EnabledMeshCount = 0;
    Models.clear();
    Models.resize(Entities.size());
    size_t i = 0;
    for (const auto& [id, Entity] : Entities)
    {
        Models[i] = &Entity->Model;
        i++;
    }

    VKSCENE::BatchInfo ModelBatch = VKSCENE::BatchModels(Models);
    VkDeviceSize VertexBufferSize = ModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
    VkDeviceSize IndexBufferSize = ModelBatch.Indices.size() * sizeof(uint32_t);
    
    std::vector<VkDrawIndexedIndirectCommand> DrawCommands;
    for (size_t i = 0; i < Entities.size(); i++)
    {
        auto& Model = Entities[i]->Model;
        for (auto& Mesh : Model.Meshes)
        {
            if (!Mesh.Enabled) continue;
            VkDrawIndexedIndirectCommand NewIndirectCommand{};
            NewIndirectCommand.indexCount = Mesh.Info.IndexCount;
            NewIndirectCommand.firstIndex = Mesh.Info.FirstIndex;
            NewIndirectCommand.instanceCount = 1;
            NewIndirectCommand.vertexOffset = Mesh.Info.VertexOffset;
            NewIndirectCommand.firstInstance = static_cast<uint32_t>(i);
            DrawCommands.push_back(NewIndirectCommand);

            EnabledMeshCount++;
        }
    }

    VKCORE::UploadDataToDeviceLocalBuffer(
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue,
        ModelBatch.Vertices.data(),
        VertexBufferSize,
        SceneVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    VKCORE::UploadDataToDeviceLocalBuffer(
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue,
        ModelBatch.Indices.data(),
        IndexBufferSize,
        SceneIndexBuffer,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );

    VKCORE::UploadDataToDeviceLocalBuffer(
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue,
        DrawCommands.data(),
        sizeof(VkDrawIndexedIndirectCommand) * DrawCommands.size(),
        SceneIndirectCommandBuffer,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
    );

    VkDeviceSize ModelMatrixesBufferSize = sizeof(glm::mat4) * Models.size();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            ModelMatrixesBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            SceneModelMatricesBuffer[i].Buffer
        );
        SceneModelMatricesBuffer[i].Map(RendererContext->DeviceContext.logicalDevice, 0, ModelMatrixesBufferSize, 0);
        
        VKCORE::DescriptorSetWriteBuffer SceneModelMatricesBufferWrite(SceneModelMatricesBuffer[i].Buffer, ModelMatrixesBufferSize,0,IndirectDescriptorSets[i],VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { SceneModelMatricesBufferWrite }, {});
    }  
}

void VKSCENE::Scene::UpdateMeshBuffers()
{
    DestroyMeshBuffers();
    CreateMeshBuffers();
}

void VKSCENE::Scene::CreateLightBuffers(uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount)
{
    VkDeviceSize DynamicLightBufferSize = sizeof(LightData) * MaxDynamicLightCount;
    VkDeviceSize StaticLightBufferSize = sizeof(LightData) * MaxStaticLightCount;

    DynamicLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    StaticLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            DynamicLightBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            DynamicLightSSBO[i].Buffer
        );
        DynamicLightSSBO[i].Map(RendererContext->DeviceContext.logicalDevice, 0, DynamicLightBufferSize, 0);

        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            StaticLightBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            StaticLightSSBO[i]
        );
    }

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        StaticLightBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        StaticLightStagingBuffer.Buffer
    );
    StaticLightStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StaticLightBufferSize, 0);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteBuffer StaticSSBOwrite(StaticLightSSBO[i], StaticLightBufferSize, 0, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::DescriptorSetWriteBuffer DynamicSSBOwrite(DynamicLightSSBO[i].Buffer, DynamicLightBufferSize, 1, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { StaticSSBOwrite,DynamicSSBOwrite }, {});
    }
}

void VKSCENE::Scene::DestroyMeshBuffers()
{
     SceneVertexBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
     SceneIndexBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
     SceneIndirectCommandBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
     for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
     {
         SceneModelMatricesBuffer[i].Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
     }
}

void VKSCENE::Scene::DestroyLightBuffers()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        StaticLightSSBO[i].Destroy(RendererContext->DeviceContext.logicalDevice);
        DynamicLightSSBO[i].Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    }
    StaticLightStagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::Scene::UpdateMeshTransformations(uint32_t CurrentFrame)
{
    glm::mat4* Destination = reinterpret_cast<glm::mat4*>(SceneModelMatricesBuffer[CurrentFrame].MappedMemory);
    for (size_t i = 0; i < Models.size(); i++)
    {
       auto& Model = Models[i];
       Destination[i] = Model->transformation.GetModelMatrix();       
    }
}

