#include "Scene.hpp"
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../app/RendererContext.hpp"
#include "Cubemap.hpp"
#include "../vkcore/VulkanPipeline.hpp"

#include "DependencyManager.hpp"
#include "MaterialManager.hpp"

VKSCENE::Scene::Scene(VKAPP::RendererContext& RendererContext, MeshUpdateModeHint MeshUpdateModeHint)
{
    Create(RendererContext, MeshUpdateModeHint);
}

void VKSCENE::Scene::Create(VKAPP::RendererContext& RendererContext, MeshUpdateModeHint MeshUpdateModeHint)
{
    this->meshUpdateModeHint = MeshUpdateModeHint;
    BalancedModeBuffers.VertexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    BalancedModeBuffers.IndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    BalancedModeBuffers.IndirectBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    PendingResourceUpdates.resize(MAX_FRAMES_IN_FLIGHT);
    //Light SSBO descriptor set
    SceneDescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3 * MAX_FRAMES_IN_FLIGHT},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MAX_FRAMES_IN_FLIGHT} },
        2 * MAX_FRAMES_IN_FLIGHT, RendererContext.DeviceContext.logicalDevice
    );
    
    SceneModelMatricesBuffer.resize(MAX_FRAMES_IN_FLIGHT);
    SceneMeshIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    SceneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.SceneDescriptorSetLayouts, SceneDescriptorSets);
    
    IndirectDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.IndirectDescriptorSetLayouts, IndirectDescriptorSets);
    
    TexturesDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    TexturesIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    TexturesIndexStagingBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    this->RendererContext = &RendererContext;
    DrawCubeMap = true;
}

void VKSCENE::Scene::Destroy()
{
    DestroyMeshTextureDescriptors();
    SceneDescriptorPool.Destroy(RendererContext->DeviceContext.logicalDevice);
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

    VkDeviceSize DynamicBufferSize = sizeof(LightData) * DynamicLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[0].MappedMemory);

    bool IsAnyUpdated = false;
    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
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

    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[CurrentFrame].MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
        {
            Destination[i] = Light->Data;
        }
        i++;
    }
}

void VKSCENE::Scene::UpdateStaticLightBuffers()
{
    if (!this->DependencyManager) return;

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
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

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
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

void VKSCENE::Scene::BatchMeshes(
    std::vector<std::pair<VKSCENE::Mesh*,uint32_t>> &Meshes,
    VKCORE::PersistentBuffer &DestinationVertexStagingBuffer,
    VKCORE::PersistentBuffer &DestinationIndexStagingBuffer,
)
{
    

    DestinationVertexStagingBuffer.Buffer
    //uint32_t BatchVertexStart = DestinationBatch.Vertices.size();
    //uint32_t TotalIndexStart = DestinationBatch.Indices.size();
    uint8_t *VertexPtr = DestinationVertexStagingBuffer
    
    uint32_t IndexOffset = 0;
    for (auto& [Mesh,ModelIndex] : Meshes)
    {
        if (!Mesh->Enabled) continue;

        Mesh->Info.VertexOffset = DestinationBatch.Vertices.size();
        DestinationBatch.Vertices.insert(DestinationBatch.Vertices.end(), Mesh->Vertices.begin(), Mesh->Vertices.end());

        Mesh->Info.IndexCount = Mesh->Indices.size();
        Mesh->Info.FirstIndex = IndexOffset;

        DestinationBatch.Indices.insert(DestinationBatch.Indices.end(), Mesh->Indices.begin(), Mesh->Indices.end());

        IndexOffset += Mesh->Indices.size();
    }
}

void VKSCENE::Scene::GetMeshBuffersSizes(
    std::vector<std::pair<VKSCENE::Mesh*, uint32_t>>& Meshes,
    uint32_t& TotalVertexBufferSize, 
    uint32_t& TotalIndexBufferSize,
    uint32_t& TotalIndirectCommandBufferSize
)
{
    uint32_t TotalVertexCount = 0;
    uint32_t TotalIndexCount = 0;
    for (auto& [Mesh, ModelIndex] : Meshes)
    {
        if (!Mesh->Enabled) continue;
        TotalVertexCount += Mesh->Vertices.size();
        TotalIndexCount += Mesh->Indices.size();
    }
}

void VKSCENE::Scene::FillMeshesArray(std::vector<uint32_t>& ModelIndexes)
{
    uint16_t QueueIndex = 0;
    for (auto& MeshQueue : Meshes)
    {
        MeshQueue.reserve(Entities.size() / 3);
    }
    for (uint32_t EntityIndex = 0; EntityIndex < Entities.size(); EntityIndex++)
    {
        const auto& Entity = Entities[EntityIndex];
        for (auto& Mesh : Entity->Model.Meshes)
        {
            Meshes[static_cast<uint16_t>(Mesh.meshUpdateMode)].push_back({ &Mesh, EntityIndex });
        }
        ModelIndexes.push_back(EntityIndex);
    }
}

void VKSCENE::Scene::ProcessMeshes(
    std::vector<std::vector<VkDrawIndexedIndirectCommand>> &DrawCommands,
    std::vector<uint32_t> &ModelIndexes,
    VKSCENE::BatchInfo &PerformanceModelBatch,
    VKSCENE::BatchInfo &BalancedModelBatch,
    VKSCENE::BatchInfo &MemorySavingModelBatch
)
{
    EnabledMeshCount = 0;
    BatchMeshes(Meshes[0], PerformanceModelBatch);
    BatchMeshes(Meshes[1], BalancedModelBatch);
    BatchMeshes(Meshes[2], MemorySavingModelBatch);
    for (size_t i = 0; i < 3; i++)
    {
        auto& MeshQueue = Meshes[i];
        for (auto& [Mesh,ModelIndex] : MeshQueue)
        {
            if (!Mesh->Enabled) continue;
            VkDrawIndexedIndirectCommand NewIndirectCommand{};
            NewIndirectCommand.indexCount = Mesh->Info.IndexCount;
            NewIndirectCommand.firstIndex = Mesh->Info.FirstIndex;
            NewIndirectCommand.instanceCount = 1;
            NewIndirectCommand.vertexOffset = Mesh->Info.VertexOffset;
            NewIndirectCommand.firstInstance = static_cast<uint32_t>(EnabledMeshCount);
            DrawCommands[i].push_back(NewIndirectCommand);

            EnabledMeshCount++;
        }
    }
}

void VKSCENE::Scene::CreatePerformanceModeBuffers(
    VkDeviceSize PerformanceVertexBufferSize,
    VkDeviceSize PerformanceIndexBufferSize,
    VkDeviceSize PerformanceIndirectCommandBufferSize
)
{
    VKCORE::CreateBuffer(RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        PerformanceVertexBufferSize * MAX_FRAMES_IN_FLIGHT,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        PerformanceModeBuffers.VertexBuffer
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        PerformanceIndexBufferSize * MAX_FRAMES_IN_FLIGHT,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        PerformanceModeBuffers.IndexBuffer
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        PerformanceIndirectCommandBufferSize * MAX_FRAMES_IN_FLIGHT,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        PerformanceModeBuffers.IndirectBuffer
    );
}

void VKSCENE::Scene::CopyDataIntoPerformanceModeBuffers(VkDeviceSize PerformanceVertexBufferSize, VkDeviceSize PerformanceIndexBufferSize, VkDeviceSize PerformanceIndirectCommandBufferSize)
{
    std::vector<VKCORE::BufferCopyInfo> BufferCopyInfos;
    std::vector<VkBufferCopy> VertexBufferCopyRegions;
    std::vector<VkBufferCopy> IndexBufferCopyRegions;
    std::vector<VkBufferCopy> IndirectBufferCopyRegions;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = i * PerformanceVertexBufferSize;
        CopyRegion.size = PerformanceVertexBufferSize;
        VertexBufferCopyRegions.push_back(CopyRegion);
        PerformanceModeBuffers.VertexOffset[i] = i * PerformanceVertexBufferSize;

        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = i * PerformanceIndexBufferSize;
        CopyRegion.size = PerformanceIndexBufferSize;
        IndexBufferCopyRegions.push_back(CopyRegion);
        PerformanceModeBuffers.IndexOffset[i] = i * PerformanceVertexBufferSize;

        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = i * PerformanceIndirectCommandBufferSize;
        CopyRegion.size = PerformanceIndirectCommandBufferSize;
        IndirectBufferCopyRegions.push_back(CopyRegion);
        PerformanceModeBuffers.IndirectOffset[i] = i * PerformanceVertexBufferSize;
    }
    BufferCopyInfos.emplace_back(VertexBufferCopyRegions, MeshStagingBuffers.VertexBuffer, PerformanceModeBuffers.VertexBuffer);
    BufferCopyInfos.emplace_back(IndexBufferCopyRegions, MeshStagingBuffers.IndexBuffer, PerformanceModeBuffers.IndexBuffer);
    BufferCopyInfos.emplace_back(IndirectBufferCopyRegions, MeshStagingBuffers.IndirectCommandBuffer, PerformanceModeBuffers.IndirectBuffer);

    VKCORE::CopyBuffer(
        BufferCopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    PerformanceModeBuffers.VertexBufferSize = MAX_FRAMES_IN_FLIGHT * PerformanceVertexBufferSize;
    PerformanceModeBuffers.IndexBufferSize = MAX_FRAMES_IN_FLIGHT * PerformanceIndexBufferSize;
    PerformanceModeBuffers.IndirectBufferSize = MAX_FRAMES_IN_FLIGHT * PerformanceIndirectCommandBufferSize;
}

void VKSCENE::Scene::CreateBalancedModeBuffers(VkDeviceSize BalancedVertexBufferSize, VkDeviceSize BalancedIndexBufferSize, VkDeviceSize BalancedIndirectCommandBufferSize)
{
    VKCORE::CreateBuffer(RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        BalancedVertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        BalancedModeBuffers.VertexBuffers[CurrentBalancedBuffer]
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        BalancedIndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        BalancedModeBuffers.IndexBuffers[CurrentBalancedBuffer]
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        BalancedIndirectCommandBufferSize,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        BalancedModeBuffers.IndirectBuffers[CurrentBalancedBuffer]
    );
}

void VKSCENE::Scene::CopyDataIntoBalancedModeBuffers(VkDeviceSize BalancedVertexBufferSize, VkDeviceSize BalancedIndexBufferSize, VkDeviceSize BalancedIndirectCommandBufferSize)
{
    std::vector<VKCORE::BufferCopyInfo> BufferCopyInfos;
    std::vector<VkBufferCopy> VertexBufferCopyRegions;
    std::vector<VkBufferCopy> IndexBufferCopyRegions;
    std::vector<VkBufferCopy> IndirectBufferCopyRegions;

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = BalancedVertexBufferSize;
    VertexBufferCopyRegions.push_back(CopyRegion);

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = BalancedIndexBufferSize;
    IndexBufferCopyRegions.push_back(CopyRegion);

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = BalancedIndirectCommandBufferSize;
    IndirectBufferCopyRegions.push_back(CopyRegion);
    
    BufferCopyInfos.emplace_back(VertexBufferCopyRegions, MeshStagingBuffers.VertexBuffer, BalancedModeBuffers.VertexBuffers[CurrentBalancedBuffer]);
    BufferCopyInfos.emplace_back(IndexBufferCopyRegions, MeshStagingBuffers.IndexBuffer, BalancedModeBuffers.IndexBuffers[CurrentBalancedBuffer]);
    BufferCopyInfos.emplace_back(IndirectBufferCopyRegions, MeshStagingBuffers.IndirectCommandBuffer, BalancedModeBuffers.IndirectBuffers[CurrentBalancedBuffer]);

    VKCORE::CopyBuffer(
        BufferCopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );
}

void VKSCENE::Scene::CreateMemorySavingModeBuffers(
    VkDeviceSize MemorySavingVertexBufferSize,
    VkDeviceSize MemorySavingIndexBufferSize,
    VkDeviceSize MemorySavingIndirectCommandBufferSize
)
{
    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MemorySavingVertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        MemorySavingModeBuffers.VertexBuffer
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MemorySavingIndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        MemorySavingModeBuffers.IndexBuffer
    );

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MemorySavingIndirectCommandBufferSize,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        MemorySavingModeBuffers.IndirectBuffer
    );
}

void VKSCENE::Scene::CopyDataIntoMemorySavingModeBuffers(VkDeviceSize MemorySavingVertexBufferSize, VkDeviceSize MemorySavingIndexBufferSize, VkDeviceSize MemorySavingIndirectCommandBufferSize)
{
    std::vector<VKCORE::BufferCopyInfo> BufferCopyInfos;
    std::vector<VkBufferCopy> VertexBufferCopyRegions;
    std::vector<VkBufferCopy> IndexBufferCopyRegions;
    std::vector<VkBufferCopy> IndirectBufferCopyRegions;

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = MemorySavingVertexBufferSize;
    VertexBufferCopyRegions.push_back(CopyRegion);

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = MemorySavingIndexBufferSize;
    IndexBufferCopyRegions.push_back(CopyRegion);

    VkBufferCopy CopyRegion{};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size = MemorySavingIndirectCommandBufferSize;
    IndirectBufferCopyRegions.push_back(CopyRegion);

    BufferCopyInfos.emplace_back(VertexBufferCopyRegions, MeshStagingBuffers.VertexBuffer, MemorySavingModeBuffers.VertexBuffer);
    BufferCopyInfos.emplace_back(IndexBufferCopyRegions, MeshStagingBuffers.IndexBuffer, MemorySavingModeBuffers.IndexBuffer);
    BufferCopyInfos.emplace_back(IndirectBufferCopyRegions, MeshStagingBuffers.IndirectCommandBuffer, MemorySavingModeBuffers.IndirectBuffer);

    VKCORE::CopyBuffer(
        BufferCopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );
}

void VKSCENE::Scene::CreateMeshBuffers()
{
    if (Entities.empty()) return;

    std::vector<std::vector<VkDrawIndexedIndirectCommand>> DrawCommands(3);
    std::vector<uint32_t> ModelIndexes;

    VKSCENE::BatchInfo PerformanceModelBatch;
    VKSCENE::BatchInfo BalancedModelBatch;
    VKSCENE::BatchInfo MemorySavingModelBatch;

    Meshes.clear();
    Meshes.resize(3);
    FillMeshesArray(ModelIndexes);

    ProcessMeshes(
        DrawCommands,
        ModelIndexes,
        PerformanceModelBatch,
        BalancedModelBatch,
        MemorySavingModelBatch
    );

    VkDeviceSize PerformanceVertexBufferSize = PerformanceModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
    VkDeviceSize PerformanceIndexBufferSize = PerformanceModelBatch.Indices.size() * sizeof(uint32_t);
    VkDeviceSize PerformanceIndirectCommandBufferSize = sizeof(VkDrawIndexedIndirectCommand) * DrawCommands[0].size();

    VkDeviceSize BalancedVertexBufferSize = BalancedModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
    VkDeviceSize BalancedIndexBufferSize = BalancedModelBatch.Indices.size() * sizeof(uint32_t);
    VkDeviceSize BalancedIndirectCommandBufferSize = sizeof(VkDrawIndexedIndirectCommand) * DrawCommands[1].size();

    VkDeviceSize MemorySavingVertexBufferSize = MemorySavingModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
    VkDeviceSize MemorySavingIndexBufferSize = MemorySavingModelBatch.Indices.size() * sizeof(uint32_t);
    VkDeviceSize MemorySavingIndirectCommandBufferSize = sizeof(VkDrawIndexedIndirectCommand) * DrawCommands[2].size();

    VkDeviceSize MeshModelIndexesBufferSize = sizeof(uint32_t) * ModelIndexes.size();

    VkDeviceSize MaxVertexBufferSize = glm::max(MeshModelIndexesBufferSize,glm::max(MemorySavingVertexBufferSize,glm::max(PerformanceVertexBufferSize, BalancedVertexBufferSize)));
    VkDeviceSize MaxIndexBufferSize = glm::max(MemorySavingIndexBufferSize, glm::max(PerformanceIndexBufferSize, BalancedIndexBufferSize));
    VkDeviceSize MaxIndirectCommandBufferSize = glm::max(PerformanceIndirectCommandBufferSize, glm::max(BalancedIndirectCommandBufferSize, MemorySavingIndirectCommandBufferSize));

    VKCORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MaxVertexBufferSize,
        MeshStagingBuffers.VertexBuffer.Buffer
    );

    MeshStagingBuffers.VertexBuffer.Map(
        RendererContext->DeviceContext.logicalDevice,
        0,
        MaxVertexBufferSize,
        0
    );


    VKCORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MaxIndexBufferSize,
        MeshStagingBuffers.IndexBuffer.Buffer
    );

    MeshStagingBuffers.IndexBuffer.Map(
        RendererContext->DeviceContext.logicalDevice,
        0,
        MaxIndexBufferSize,
        0
    );

    VKCORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        MaxIndirectCommandBufferSize,
        MeshStagingBuffers.IndirectCommandBuffer.Buffer
    );

    MeshStagingBuffers.IndirectCommandBuffer.Map(
        RendererContext->DeviceContext.logicalDevice,
        0,
        MaxIndirectCommandBufferSize,
        0
    );
    memcpy(MeshStagingBuffers.VertexBuffer.MappedMemory, PerformanceModelBatch.Vertices.data(), PerformanceVertexBufferSize);
    memcpy(MeshStagingBuffers.IndexBuffer.MappedMemory, PerformanceModelBatch.Indices.data(), PerformanceIndexBufferSize);
    memcpy(MeshStagingBuffers.IndirectCommandBuffer.MappedMemory, PerformanceModelBatch.Indices.data(), PerformanceIndirectCommandBufferSize);

    if (PerformanceVertexBufferSize && PerformanceIndexBufferSize && PerformanceIndirectCommandBufferSize)
    {
        CreatePerformanceModeBuffers(PerformanceVertexBufferSize, PerformanceIndexBufferSize, PerformanceIndirectCommandBufferSize);
        CopyDataIntoPerformanceModeBuffers(PerformanceVertexBufferSize, PerformanceIndexBufferSize, PerformanceIndirectCommandBufferSize);
    }

    memcpy(MeshStagingBuffers.VertexBuffer.MappedMemory, BalancedModelBatch.Vertices.data(), BalancedVertexBufferSize);
    memcpy(MeshStagingBuffers.IndexBuffer.MappedMemory, BalancedModelBatch.Indices.data(), BalancedIndexBufferSize);
    memcpy(MeshStagingBuffers.IndirectCommandBuffer.MappedMemory, BalancedModelBatch.Indices.data(), BalancedIndirectCommandBufferSize);

    if (BalancedVertexBufferSize && BalancedIndexBufferSize && BalancedIndirectCommandBufferSize)
    {
        CreateBalancedModeBuffers(BalancedVertexBufferSize, BalancedIndexBufferSize, BalancedIndirectCommandBufferSize);
        CopyDataIntoBalancedModeBuffers(BalancedVertexBufferSize, BalancedIndexBufferSize, BalancedIndirectCommandBufferSize);
    }

    memcpy(MeshStagingBuffers.VertexBuffer.MappedMemory, MemorySavingModelBatch.Vertices.data(), MemorySavingVertexBufferSize);
    memcpy(MeshStagingBuffers.IndexBuffer.MappedMemory, MemorySavingModelBatch.Indices.data(), MemorySavingIndexBufferSize);
    memcpy(MeshStagingBuffers.IndirectCommandBuffer.MappedMemory, MemorySavingModelBatch.Indices.data(), MemorySavingIndirectCommandBufferSize);

    if (MemorySavingVertexBufferSize && MemorySavingIndexBufferSize && MemorySavingIndirectCommandBufferSize)
    {
        CreateMemorySavingModeBuffers(MemorySavingVertexBufferSize,MemorySavingIndexBufferSize,MemorySavingIndirectCommandBufferSize);
        CopyDataIntoMemorySavingModeBuffers(MemorySavingVertexBufferSize, MemorySavingIndexBufferSize, MemorySavingIndirectCommandBufferSize);
    }
   
    std::vector<VKCORE::BufferCopyInfo> BufferCopyInfos;
    std::vector<VKCORE::DescriptorSetWriteBuffer> DescriptorBufferWrites;
    VkDeviceSize ModelMatrixesBufferSize = sizeof(glm::mat4) * Meshes.size();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            MeshModelIndexesBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            SceneMeshIndexBuffers[i]
        );

        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = 0;
        CopyRegion.size = MeshModelIndexesBufferSize;
        BufferCopyInfos.push_back({ { CopyRegion }, MeshStagingBuffers.VertexBuffer.Buffer.BufferObject, SceneMeshIndexBuffers[i].BufferObject });

        VKCORE::CreateBuffer(RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            ModelMatrixesBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            SceneModelMatricesBuffer[i].Buffer
        );
        SceneModelMatricesBuffer[i].Map(RendererContext->DeviceContext.logicalDevice, 0, ModelMatrixesBufferSize, 0);
        
        DescriptorBufferWrites.emplace_back(SceneModelMatricesBuffer[i].Buffer, ModelMatrixesBufferSize, 0, IndirectDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        DescriptorBufferWrites.emplace_back(SceneMeshIndexBuffers[i], ModelMatrixesBufferSize, 1, IndirectDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }  
    VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, DescriptorBufferWrites, {});

    VKCORE::CopyBuffer(
        BufferCopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    MeshStagingBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::Scene::UpdateMeshBuffers()
{
    DestroyMeshBuffers();
    CreateMeshBuffers();
}

template<typename... args>
void VKSCENE::Scene::RequestMultipleFramesUpdate(ScenePendingUpdateType UpdateType,SceneResourceUpdateCallback &UpdateCallback, args&&... UpdateFunctionArguments)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        PendingResourceUpdates[i].push_back({ UpdateType,&UpdateCallback });
    }
}

void VKSCENE::Scene::RequestSingleFrameUpdate(ScenePendingUpdateType UpdateType, uint32_t FrameIndex, SceneResourceUpdateCallback& UpdateCallback)
{
    PendingResourceUpdates[FrameIndex].push_back({ UpdateType,&UpdateCallback });
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
    PerformanceModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    BalancedModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    MemorySavingModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        SceneModelMatricesBuffer[i].Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        SceneMeshIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
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

void VKSCENE::Scene::HandleUpdateRequests(uint32_t CurrentFrame)
{
    for (auto& Request : PendingResourceUpdates[CurrentFrame])
    {
        switch (Request.first)
        {
        case SCENE_PENDING_UPDATE_TYPE_STATIC_LIGHT_BUFFERS:
        {
            //UpdateStaticFrameLightBuffers();
            break;
        }
        default:
            break;
        }
    }
    
}

void VKSCENE::Scene::UpdateMeshTransformations(uint32_t CurrentFrame)
{
    glm::mat4* Destination = reinterpret_cast<glm::mat4*>(SceneModelMatricesBuffer[CurrentFrame].MappedMemory);
    for (size_t i = 0; i < Meshes.size(); i++)
    {
       auto& Model = Meshes[i];
       Destination[i] = Model->transformation.GetModelMatrix();       
    }
}

void VKSCENE::Scene::CreateMeshTextureDescriptors(
    uint32_t MaxTextures
)
{
    TexturesDescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MaxTextures}, 
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1} },
        1,
        RendererContext->DeviceContext.logicalDevice,
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
    );

    VkDescriptorBindingFlags LayoutFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo BindingFlags{};
    BindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    BindingFlags.pBindingFlags = &LayoutFlags;
    BindingFlags.bindingCount = 1;

    TexturesDescriptorSetLayout.AppendLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        MaxTextures,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    TexturesDescriptorSetLayout.AppendLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    TexturesDescriptorSetLayout.CreateLayout(
        RendererContext->DeviceContext.logicalDevice,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        &BindingFlags
    );

    VKCORE::AllocateDescriptorSets(
        RendererContext->DeviceContext.logicalDevice,
        TexturesDescriptorSets.size(),
        TexturesDescriptorPool.descriptorPool,
        TexturesDescriptorSetLayout.descriptorSetLayout,
        TexturesDescriptorSets
    );

    uint32_t BlockSize = 250;
    ActualTextureUpperBound = static_cast<uint32_t>(glm::ceil((float)MaxTextures / (float)BlockSize)) * BlockSize;
    std::cout << "Creating texture descriptor set with upper bound: " << ActualTextureUpperBound << "\n";
    CurrentGbufferPassPipeline = RendererContext->AppendGbufferPassPipeline(TexturesDescriptorSetLayout.descriptorSetLayout, ActualTextureUpperBound);

    VkDeviceSize TexturesIndexBufferSize = sizeof(int) * ActualTextureUpperBound;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            TexturesIndexBuffers[i]
        );

        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            TexturesIndexStagingBuffers[i]
        );

        VKCORE::DescriptorSetWriteBuffer TexturesIndexSSBOWrite(
            TexturesIndexBuffers[i],
            TexturesIndexBufferSize,
            1,
            TexturesDescriptorSets[i],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { TexturesIndexSSBOWrite }, {});
    }
}

void VKSCENE::Scene::DestroyMeshTextureDescriptors()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TexturesIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
        TexturesIndexStagingBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
    }
    TexturesDescriptorSetLayout.Destroy(RendererContext->DeviceContext.logicalDevice);
    TexturesDescriptorPool.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::Scene::WriteTexture(
    MaterialTextureType TextureType,
    VKSCENE::Mesh& Mesh,
    VKSCENE::TextureImportManager& TextureImportManager,
    std::vector<VKCORE::DescriptorSetWriteImage> &ImageWrites,
    std::vector<int> &TextureIndexes,
    uint32_t CurrentImageIndex
)
{
    auto TextureIndex = Mesh.MeshMaterial.GetTexture(TextureType);
    if (TextureIndex)
    {
        auto Iterator = TextureImportManager.TextureDatas.find(TextureIndex);
        if (Iterator == TextureImportManager.TextureDatas.end())
        {
            TextureIndexes.push_back(-1);
            return;
        }
        auto& TextureData = Iterator->second;

        std::cout << "WRITING IMAGE WITH INDEX: " << CurrentImageIndex << std::endl;
        for (size_t i = 0; i < TexturesDescriptorSets.size(); i++)
        {
            ImageWrites.emplace_back(
                TextureData.ImageView,
                TextureData.Sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                0,
                TexturesDescriptorSets[i],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                CurrentImageIndex,
                1
            );
        }
        TextureIndexes.push_back(CurrentImageIndex);
    }
    else TextureIndexes.push_back(-1);
    CurrentImageIndex++;
}

void VKSCENE::Scene::UpdateTextureDescriptors(VKSCENE::TextureImportManager &TextureImportManager)
{
    std::vector<VKCORE::DescriptorSetWriteImage> ImageWrites;
    std::vector<int> TextureIndexes;
    uint32_t CurrentImageIndex = 0;
    for (auto& [ID,Entity] : Entities)
    {
        for (auto& Mesh : Entity->Model.Meshes)
        {
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_ALBEDO,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_ROUGHNESS,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_METALLIC,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_NORMAL_MAP,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_OPACITY,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex
            );
        }
    }
    VKCORE::WriteDescriptorSets(
        RendererContext->DeviceContext.logicalDevice, 
        {}, 
        ImageWrites
    );

    VkDeviceSize BufferSize = sizeof(int) * TextureIndexes.size();
    void* DataPtr;
    for (size_t i = 0; i < TexturesIndexStagingBuffers.size(); i++)
    {
        vkMapMemory(
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexStagingBuffers[i].BufferMemory,
            0,
            BufferSize,
            0,
            &DataPtr
        );
        memcpy(DataPtr, TextureIndexes.data(), BufferSize);
        vkUnmapMemory(RendererContext->DeviceContext.logicalDevice, TexturesIndexStagingBuffers[i].BufferMemory);

        VKCORE::CopyBuffer(
            TexturesIndexStagingBuffers[i].BufferObject,
            TexturesIndexBuffers[i].BufferObject,
            BufferSize,
            RendererContext->DeviceContext.logicalDevice,
            RendererContext->CommandPool.commandPool,
            RendererContext->DeviceContext.GraphicsQueue
        );
    }
}

