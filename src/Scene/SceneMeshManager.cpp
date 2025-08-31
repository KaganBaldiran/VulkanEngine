#include "SceneMeshManager.hpp"

#include "Mesh.hpp"
#include "ModelInstance.hpp"
#include "../Renderer/RendererContext.hpp"
#include "MaterialManager.hpp"
#include "Scene.hpp"
#include "MeshManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

enum BufferCopySlots {
    INDIRECT_COPY = 0,
    DRAWMETA_COPY = 1
};

SCENE::SceneMeshManager::SceneMeshManager(MeshManager& MeshManager, RENDERER::RendererContext &RendererContext)
{
    Create(MeshManager,RendererContext);
}

void SCENE::SceneMeshManager::Create(MeshManager& MeshManager, RENDERER::RendererContext& RendererContext)
{
    this->RendererContext = &RendererContext;
    this->MeshManagerPtr = &MeshManager;
    SceneMeshIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    PerformanceModeBuffers.Create();
}

void SCENE::SceneMeshManager::Destroy(VkDevice& LogicalDevice)
{
    PerformanceModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //BalancedModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //MemorySavingModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        SceneMeshIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
    }
}

//Update mesh transformation buffers based on the update list
void SCENE::SceneMeshManager::UpdateMeshTransformations(std::vector<ModelInstance*>& UpdateList, uint32_t CurrentFrame)
{
    if (UpdateList.empty()) return;

    auto& ModelMatrixBuffer = PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame].Buffer;
    uint8_t* Destination = reinterpret_cast<uint8_t*>(ModelMatrixBuffer.MappedMemory);
    auto& CurrentFrameInstanceEntries = ModelEntries[CurrentFrame].InstanceEntries;

    std::vector<VkMappedMemoryRange> MemoryRanges;
    MemoryRanges.reserve(UpdateList.size());
    for (auto& ModelInstance : UpdateList)
    {
        auto& ModelInstanceEntryIterator = CurrentFrameInstanceEntries.find(ModelInstance->ResourceID);
        if (ModelInstanceEntryIterator == CurrentFrameInstanceEntries.end()) continue;

        auto& AllocatedMemoryRegion = ModelInstanceEntryIterator->second.TransformationMatrixMemoryRegion;
        glm::mat4 ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
        memcpy(Destination + AllocatedMemoryRegion.Offset,
            &ModelMatrix,
            AllocatedMemoryRegion.Size
        );

        VkMappedMemoryRange MemoryRange{};
        MemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        MemoryRange.offset = AllocatedMemoryRegion.Offset;
        MemoryRange.memory = ModelMatrixBuffer.Buffer.BufferMemory;
        MemoryRange.size = AllocatedMemoryRegion.Size;
        MemoryRanges.push_back(std::move(MemoryRange));
    }

    vkFlushMappedMemoryRanges(RendererContext->DeviceContext.logicalDevice, static_cast<uint32_t>(MemoryRanges.size()), MemoryRanges.data());
}

//Fetch correct materials 
std::unordered_map<size_t,SCENE::MaterialMetaData> CreateMaterialMetaData(
    SCENE::ModelInstance* ModelInstance,
    RENDERER_CORE::VirtualArenaAllocator& TexturesIndexBufferAllocator,
    size_t MaterialTextureTypeCount,
    std::unordered_map<size_t, SCENE::GeometryEntry> &CurrentFrameGeometryEntries
)
{
    std::unordered_map<size_t, SCENE::MaterialMetaData> MaterialMetaDataList;
    MaterialMetaDataList.reserve(ModelInstance->Source->Meshes.size());
   
    for (size_t i = 0; i < ModelInstance->Source->Meshes.size(); i++)
    {
        SCENE::MaterialMetaData NewMaterialMetaData{};
        
        if (ModelInstance->Materials.size() > i)
        {
            NewMaterialMetaData.Material = ModelInstance->Materials[i];
        }
        else
        {
            auto& GeometryEntryIterator = CurrentFrameGeometryEntries.find(ModelInstance->Source->Meshes[i].GeometryID);
            if (GeometryEntryIterator == CurrentFrameGeometryEntries.end())
            {
                throw std::runtime_error("Attempt on linking non-existent mesh instance!");
            }
            NewMaterialMetaData.Material = GeometryEntryIterator->second.MeshMaterial;
        }

        NewMaterialMetaData.TextureIndexMemoryRegion =
            TexturesIndexBufferAllocator.Allocate(MaterialTextureTypeCount * sizeof(int));

        MaterialMetaDataList[ModelInstance->Source->Meshes[i].GeometryID] = (std::move(NewMaterialMetaData));
    }

    return MaterialMetaDataList;
}

void SCENE::SceneMeshManager::AppendModels(MeshAppendInfo Info)
{
    auto& CurrentFrame = Info.FrameIndex;
    this->MeshManagerPtr->UpdateGeometryEntries(CurrentFrame);

    auto& EnabledMeshCount = PerformanceModeBuffers.EnabledMeshCount[CurrentFrame];

    auto& IndirectBuffer = PerformanceModeBuffers.IndirectBuffers[CurrentFrame];
    auto& TexturesIndexBuffer = PerformanceModeBuffers.TexturesIndexBuffers[CurrentFrame];

    auto& CullBuffers = PerformanceModeBuffers.CullBuffers;
    auto& CulledMetaDataBuffer = CullBuffers.CulledDrawMetaDataBuffer[CurrentFrame];
    auto& CulledIndirectBuffer = CullBuffers.CulledIndirectBuffers[CurrentFrame];
    auto& MeshVisibilityCountBuffer = CullBuffers.MeshVisibilityCountBuffers[CurrentFrame];

    auto& DrawMetaDataBuffer = PerformanceModeBuffers.DrawMetaDataBuffer[CurrentFrame];
    auto& ModelMatricesBuffer = PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame];

    auto& IndirectBufferAllocator = IndirectBuffer.Allocator;
    auto& ModelMatricesBufferAllocator = ModelMatricesBuffer.Allocator;
    auto& DrawMetaDataBufferAllocator = DrawMetaDataBuffer.Allocator;
    auto& MeshVisibilityCountBufferAllocator = MeshVisibilityCountBuffer.Allocator;
    auto& TexturesIndexBufferAllocator = TexturesIndexBuffer.Allocator;

    auto& TexturesIndexBufferReallocated = PerformanceModeBuffers.TexturesIndexBuffersReallocated[CurrentFrame];

    size_t VertexSize, IndexSize,IndirectSize, VertexBufferSize = 0,IndexBufferSize = 0,IndirectBufferSize = 0;
    size_t SizeOfVertex = sizeof(Vertex3D), 
        SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4), 
        SizeOfDrawMetaData = sizeof(DrawMetadata), 
        SizeOfIndirectCommand = sizeof(ExtendedIndirectCommand);

    size_t  IndirectBufferCapacity = IndirectBufferAllocator.GetCapacity(),
            ModelMatricesBufferCapacity = ModelMatricesBufferAllocator.GetCapacity(),
            MeshVisibilityCountBufferCapacity = MeshVisibilityCountBufferAllocator.GetCapacity(),
            TexturesIndexBufferCapacity = TexturesIndexBufferAllocator.GetCapacity(),
            DrawMetaDataBufferCapacity = DrawMetaDataBufferAllocator.GetCapacity();

    size_t InsertedInstanceCount = 0 , InsertedMeshInstanceCount = 0;

    uint32_t MaterialTextureTypeCount = static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE);
    //auto& CurrentFrameMeshEntries = MeshEntries[CurrentFrame];
    auto& CurrentFrameEntries = ModelEntries[CurrentFrame];
    auto& CurrentFrameGeometryEntries = this->MeshManagerPtr->GeometryEntries[CurrentFrame];
    //Process newly inserted meshes.
    std::set<size_t> InsertedMetaDatas;
    for (auto& ModelInstance : Info.ModelInstances)
    {
        if (!ModelInstance || !ModelInstance->Source) continue;

        if (ModelInstance->Materials.size() > ModelInstance->Source->Meshes.size())
        {
            throw std::runtime_error("Instance material count exceeds target mesh count!");
        }

        CurrentFrameEntries.MaterialUpdateList.push_back(ModelInstance);
        //Handle already existing instance case
        auto InstanceIterator = CurrentFrameEntries.InstanceEntries.find(ModelInstance->ResourceID);
        if (InstanceIterator != CurrentFrameEntries.InstanceEntries.end())
        {
            for (size_t i = 0; i < ModelInstance->Source->Meshes.size(); i++)
            {
                size_t MeshGeometryID = ModelInstance->Source->Meshes[i].GeometryID;
                if (ModelInstance->Materials.size() > i)
                {
                    InstanceIterator->second.Materials[MeshGeometryID].Material = ModelInstance->Materials[i];
                }
                else
                {
                    auto& GeometryEntryIterator = CurrentFrameGeometryEntries.find(ModelInstance->Source->Meshes[i].GeometryID);
                    if (GeometryEntryIterator == CurrentFrameGeometryEntries.end())
                    {
                        throw std::runtime_error("Attempt on linking non-existent mesh instance!");
                    }
                    InstanceIterator->second.Materials[MeshGeometryID].Material = GeometryEntryIterator->second.MeshMaterial;
                }
            }
            continue;
        }
        InstanceEntry NewInstanceEntry{};
        NewInstanceEntry.TransformationMatrixMemoryRegion = ModelMatricesBufferAllocator.Allocate(SizeOfMat4);
        NewInstanceEntry.Materials = CreateMaterialMetaData(ModelInstance, TexturesIndexBufferAllocator, MaterialTextureTypeCount,CurrentFrameGeometryEntries);

        //NewInstanceEntry.MeshLinks.reserve(ModelInstance->Source->Meshes.size());
        //Process and allocate the individual meshes 
        for (auto& Mesh : ModelInstance->Source->Meshes)
        {
            //NewInstanceEntry.MeshLinks.push_back({ Mesh.GeometryID, RENDERER_CORE::MemoryRegion() });
            InsertedMetaDatas.insert(Mesh.GeometryID);

            auto& MeshEntryIterator = CurrentFrameEntries.MeshEntries.find(Mesh.GeometryID);
            bool DoesMeshAlreadyExist = MeshEntryIterator != CurrentFrameEntries.MeshEntries.end();
            if (DoesMeshAlreadyExist)
            {
                MeshEntryIterator->second.IsChanged = true;
                MeshEntryIterator->second.ReferenceCount++;
                MeshEntryIterator->second.InstanceLinks.push_back({ ModelInstance->ResourceID, RENDERER_CORE::MemoryRegion() });
                continue;
            }
            auto& GeometryEntryIterator = CurrentFrameGeometryEntries.find(Mesh.GeometryID);
            if (GeometryEntryIterator == CurrentFrameGeometryEntries.end())
            {
                throw std::runtime_error("Attempt on linking non-existent mesh instance!");
            }

            //Fill data in the new mesh entry
            MeshEntry NewMeshEntry{};
            NewMeshEntry.BoundingBox = GeometryEntryIterator->second.BoundingBox;
            NewMeshEntry.ReferenceCount++;
            NewMeshEntry.IsChanged = true;
            NewMeshEntry.InstanceLinks.push_back({ ModelInstance->ResourceID, RENDERER_CORE::MemoryRegion() });

            DrawInfo MeshDrawInfo{};
            MeshDrawInfo.VertexOffset = GeometryEntryIterator->second.VertexRegion.Offset / SizeOfVertex;
            MeshDrawInfo.FirstIndex = GeometryEntryIterator->second.IndexRegion.Offset / SizeOfUint32;
            MeshDrawInfo.IndexCount = GeometryEntryIterator->second.IndexRegion.Size / SizeOfUint32;
            NewMeshEntry.Info = MeshDrawInfo;

            NewMeshEntry.IndirectBufferMemoryRegion = IndirectBufferAllocator.Allocate(SizeOfIndirectCommand);
            NewMeshEntry.ResourceID = Mesh.GeometryID;
 
            CurrentFrameEntries.MeshEntries[Mesh.GeometryID] = NewMeshEntry;
            IndirectBufferSize += SizeOfIndirectCommand;

            EnabledMeshCount++;
        }
        CurrentFrameEntries.InstanceEntries[ModelInstance->ResourceID] = NewInstanceEntry;
        InsertedMeshInstanceCount += ModelInstance->Source->Meshes.size(); 
    }
    //ModelMatricesBufferAllocator.Allocate(SizeOfMat4 * InsertedInstanceCount);
    DrawMetaDataBufferAllocator.Allocate(InsertedMeshInstanceCount * SizeOfDrawMetaData);
    MeshVisibilityCountBufferAllocator.Allocate(InsertedMetaDatas.size() * SizeOfUint32);

    //Check whether the allocator allocated extra virtual memory
    bool IsIndirectBufferReallocated = IndirectBufferCapacity < IndirectBufferAllocator.GetCapacity(),
        IsModelMatrixesBufferReallocated = ModelMatricesBufferCapacity < ModelMatricesBufferAllocator.GetCapacity(),
        IsMeshVisibilityCountBufferReallocated = MeshVisibilityCountBufferCapacity < MeshVisibilityCountBufferAllocator.GetCapacity(),
        IsDrawMetaDataBufferReallocated = DrawMetaDataBufferCapacity < DrawMetaDataBufferAllocator.GetCapacity();

    TexturesIndexBufferReallocated = TexturesIndexBufferCapacity < TexturesIndexBufferAllocator.GetCapacity();

    std::vector<RENDERER_CORE::BufferCopyInfo> CopyInfos(2);
    std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> WriteInfos(3);

    //No more work to do. Return
    if (!InsertedMeshInstanceCount && !IsIndirectBufferReallocated && 
        !IsModelMatrixesBufferReallocated && !IsDrawMetaDataBufferReallocated && !TexturesIndexBufferReallocated)
    {
        return;
    }
    //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
    if (IsIndirectBufferReallocated)
    {
        RecreateBuffer(
            RendererContext,
            IndirectBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            IndirectBuffer.Buffer
        );

        RecreateBuffer(
            RendererContext,
            IndirectBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CulledIndirectBuffer
        );

        RENDERER_CORE::DescriptorSetWriteBuffer IndirectBufferWrite(
            IndirectBuffer.Buffer,
            IndirectBufferAllocator.GetCapacity(),
            1,
            Info.TargetDescriptorSets[CurrentFrame],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { IndirectBufferWrite }, {});
    }
    if (IsModelMatrixesBufferReallocated)
    {
        RecreateBuffer(
            RendererContext,
            ModelMatricesBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            ModelMatricesBuffer.Buffer.Buffer
        );
        ModelMatricesBuffer.Buffer.Map(RendererContext->DeviceContext.logicalDevice, 0, ModelMatricesBufferAllocator.GetCapacity(), 0);

        RENDERER_CORE::DescriptorSetWriteBuffer ModelMatrixBufferWrite(
            ModelMatricesBuffer.Buffer.Buffer,
            ModelMatricesBufferAllocator.GetCapacity(), 
            0, 
            Info.TargetDescriptorSets[CurrentFrame], 
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { ModelMatrixBufferWrite }, {});
    }
    if (IsDrawMetaDataBufferReallocated)
    {
        RecreateBuffer(
            RendererContext,
            DrawMetaDataBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            DrawMetaDataBuffer.Buffer
        );

        RecreateBuffer(
            RendererContext,
            DrawMetaDataBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CulledMetaDataBuffer
        );

        RENDERER_CORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
            DrawMetaDataBuffer.Buffer,
            DrawMetaDataBufferAllocator.GetCapacity(),
            2,
            Info.TargetDescriptorSets[CurrentFrame],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { DrawMetaDataBufferWrite }, {});
    }
    if (IsMeshVisibilityCountBufferReallocated)
    {
        RecreateBuffer(
            RendererContext,
            MeshVisibilityCountBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            MeshVisibilityCountBuffer.Buffer
        );
    }
        
    size_t IndirectStagingBufferSize = IsIndirectBufferReallocated ? IndirectBufferAllocator.GetCapacity() : IndirectBufferSize,
           DrawMetaDataBufferSize = DrawMetaDataBufferAllocator.GetCapacity();
        
    size_t TotalStagingBufferSize = IndirectStagingBufferSize + DrawMetaDataBufferSize;
    //Nothing else to do. Return
    if (!TotalStagingBufferSize)
    {
        return;
    }

    RENDERER_CORE::PersistentBuffer StagingBuffer{};
    RENDERER_CORE::VirtualArenaAllocator StagingBufferAllocator(TotalStagingBufferSize);
    RENDERER_CORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        TotalStagingBufferSize,
        StagingBuffer.Buffer
    );
    StagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, TotalStagingBufferSize, 0);
    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.MappedMemory);

    if (!StagingBufferPtr) throw std::runtime_error("Unable to map the staging buffer! exitting...");

    DrawMetaDataBufferAllocator.Reset();
    size_t MeshIterator = 0;
    //Global draw index iterator
    size_t DrawIndexIterator = 0;
    //Create or update indirect commands 
    for (auto& [Handle, MeshEntry] : CurrentFrameEntries.MeshEntries)
    {
        //Redistribute draw meta datas
        for (size_t i = 0; i < MeshEntry.InstanceLinks.size(); i++)
        {
            InstanceMeshLink& InstanceLink = MeshEntry.InstanceLinks[i];

            auto& InstanceIterator = CurrentFrameEntries.InstanceEntries.find(InstanceLink.ResourceID);
            if (InstanceIterator == CurrentFrameEntries.InstanceEntries.end()) continue;

            auto& MaterialDataIterator = InstanceIterator->second.Materials.find(Handle);

            DrawMetadata NewDrawMetaData{};
            NewDrawMetaData.MeshID = MaterialDataIterator->second.TextureIndexMemoryRegion.Offset / (MaterialTextureTypeCount * sizeof(int));
            NewDrawMetaData.ModelMatrixIndex = InstanceIterator->second.TransformationMatrixMemoryRegion.Offset / SizeOfMat4;

            RENDERER_CORE::MemoryRegion DrawMetaDataAllocatedRegion = DrawMetaDataBufferAllocator.Allocate(SizeOfDrawMetaData);
            RENDERER_CORE::MemoryRegion DrawMetaDataStagingAllocatedRegion = StagingBufferAllocator.Allocate(SizeOfDrawMetaData);
            InstanceLink.DrawDataMemoryRegion = DrawMetaDataAllocatedRegion;

            memcpy(StagingBufferPtr + DrawMetaDataStagingAllocatedRegion.Offset, &NewDrawMetaData, DrawMetaDataAllocatedRegion.Size);

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = DrawMetaDataAllocatedRegion.Offset;
            CopyRegion.size = DrawMetaDataAllocatedRegion.Size;
            CopyRegion.srcOffset = DrawMetaDataStagingAllocatedRegion.Offset;
            CopyInfos[DRAWMETA_COPY].CopyRegions.push_back(CopyRegion);
        }

        bool IsMeshJustInserted = InsertedMetaDatas.find(MeshEntry.ResourceID) != InsertedMetaDatas.end();
        if (MeshEntry.IsChanged || IsIndirectBufferReallocated || IsMeshJustInserted)
        {
            MeshEntry.IsChanged = false;

            ExtendedIndirectCommand NewIndirectCommand{};
            NewIndirectCommand.IndexCount = MeshEntry.Info.IndexCount;
            NewIndirectCommand.FirstIndex = MeshEntry.Info.FirstIndex;
            NewIndirectCommand.InstanceCount = MeshEntry.ReferenceCount;
            NewIndirectCommand.VertexOffset = MeshEntry.Info.VertexOffset;
            NewIndirectCommand.FirstInstance = DrawIndexIterator;
            NewIndirectCommand.BoundingBox = MeshEntry.BoundingBox;

            RENDERER_CORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(SizeOfIndirectCommand);
            memcpy(StagingBufferPtr + AllocatedRegion.Offset, &NewIndirectCommand, AllocatedRegion.Size);

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = MeshEntry.IndirectBufferMemoryRegion.Offset;
            CopyRegion.size = AllocatedRegion.Size;
            CopyRegion.srcOffset = AllocatedRegion.Offset;
            CopyInfos[INDIRECT_COPY].CopyRegions.push_back(CopyRegion);
        }
        DrawIndexIterator += MeshEntry.ReferenceCount;
        MeshIterator++;
    }
    CopyInfos[INDIRECT_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[INDIRECT_COPY].DestinationBuffer = IndirectBuffer.Buffer.BufferObject;
    CopyInfos[DRAWMETA_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[DRAWMETA_COPY].DestinationBuffer = DrawMetaDataBuffer.Buffer.BufferObject;
    
    //Execute copy operations 
    RENDERER_CORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.logicalDevice, 
        RendererContext->CommandPool.commandPool, 
        RendererContext->DeviceContext.GraphicsQueue
    );

    StagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void SCENE::SceneMeshManager::EraseModels(MeshEraseInfo Info)
{
    /*
    auto& CurrentFrame = Info.FrameIndex;

    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame];
    if (!CurrentFrameModelEntries.ModelEntries.size())
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING, "Invalid model erasing operation. No existing model detected. Returning...");
        return;
    }

    auto& EnabledMeshCount = PerformanceModeBuffers.EnabledMeshCount[CurrentFrame];

    auto& VertexBuffer = PerformanceModeBuffers.VertexBuffers[CurrentFrame];
    auto& IndexBuffer = PerformanceModeBuffers.IndexBuffers[CurrentFrame];
    auto& IndirectBuffer = PerformanceModeBuffers.IndirectBuffers[CurrentFrame];
    auto& ModelMatricesBuffer = PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame];
    auto& DrawMetaDataBuffer = PerformanceModeBuffers.DrawMetaDataBuffer[CurrentFrame];

    auto& VertexBufferAllocator = VertexBuffer.Allocator;
    auto& IndexBufferAllocator = IndexBuffer.Allocator;
    auto& IndirectBufferAllocator = IndirectBuffer.Allocator;
    auto& ModelMatricesBufferAllocator = ModelMatricesBuffer.Allocator;
    auto& DrawMetaDataBufferAllocator = DrawMetaDataBuffer.Allocator;

    size_t SizeOfDrawMetaData = sizeof(DrawMetadata) , SizeOfMat4 = sizeof(glm::mat4);

    //Check if any of the indirect commands are changed
    bool AreIndirectCommandsChanged = false;
    for (auto& ModelInstancePtr : Info.ModelInstances)
    {
        if (!ModelInstancePtr || !ModelInstancePtr->Source) continue;

        auto& ModelIterator = CurrentFrameModelEntries.ModelEntries.find(ModelInstancePtr->Source);
        if (ModelIterator == CurrentFrameModelEntries.ModelEntries.end()) continue;
        ModelIterator->second.MetaData.IsChanged = false;

        auto& ModelInstanceIterator = ModelIterator->second.Instances.find(ModelInstancePtr);
        if (ModelInstanceIterator == ModelIterator->second.Instances.end()) continue;

        ModelMatricesBufferAllocator.Free(ModelInstanceIterator->second[0]);
        ModelIterator->second.MetaData.ReferenceCount--;
        DrawMetaDataBufferAllocator.Free(ModelInstanceIterator->second[1]);
        ModelIterator->second.Instances.erase(ModelInstancePtr);

        if (ModelIterator->second.MetaData.ReferenceCount)
        {
            ModelIterator->second.MetaData.IsChanged = true;
            AreIndirectCommandsChanged = true;
            continue;
        }

        for (auto& MeshEntry : ModelIterator->second.MeshEntries)
        {
            const RENDERER_CORE::MemoryRegion& VertexMemoryRegion = MeshEntry.MetaData.MemoryRegions[0];
            const RENDERER_CORE::MemoryRegion& IndexMemoryRegion = MeshEntry.MetaData.MemoryRegions[1];
            const RENDERER_CORE::MemoryRegion& IndirectMemoryRegion = MeshEntry.MetaData.MemoryRegions[2];

            VertexBufferAllocator.Free(VertexMemoryRegion);
            IndexBufferAllocator.Free(IndexMemoryRegion);
            IndirectBufferAllocator.Free(IndirectMemoryRegion);

            EnabledMeshCount--;
        }
        CurrentFrameModelEntries.ModelEntries.erase(ModelInstancePtr->Source);
    }

    //Return in case all the meshes were erased.
    if (!CurrentFrameModelEntries.ModelEntries.size()) return;

    //Decide whether defragmentation is required or not
    double VertexFragmentationPercent = VertexBufferAllocator.GetFragmentationPercent(),
        IndexFragmentationPercent = IndexBufferAllocator.GetFragmentationPercent(),
        IndirectFragmentationPercent = IndirectBufferAllocator.GetFragmentationPercent();

    double Threshold = 0.8;
    bool VertexBufferNeedsDefragmentation = VertexFragmentationPercent > Threshold,
        IndexBufferNeedsDefragmentation = IndexFragmentationPercent > Threshold,
        IndirectBufferNeedsDefragmentation = IndirectFragmentationPercent > 0.0;

    bool ShouldDefragment = VertexBufferNeedsDefragmentation || IndexBufferNeedsDefragmentation || IndirectBufferNeedsDefragmentation;
    
    std::vector<RENDERER_CORE::MemoryRegion*> VertexAllocatedRegions;
    std::vector<RENDERER_CORE::MemoryRegion*> IndexAllocatedRegions;
    std::vector<RENDERER_CORE::MemoryRegion*> IndirectCommandAllocatedRegions;
    VertexAllocatedRegions.reserve(EnabledMeshCount);
    IndexAllocatedRegions.reserve(EnabledMeshCount);
    IndirectCommandAllocatedRegions.reserve(EnabledMeshCount);

    size_t VertexStagingBufferSize = VertexBufferAllocator.GetCapacity() - VertexBufferAllocator.GetTotalFreeSpace(),
        IndexStagingBufferSize = IndexBufferAllocator.GetCapacity() - IndexBufferAllocator.GetTotalFreeSpace(),
        IndirectStagingBufferSize = IndirectBufferAllocator.GetCapacity() - IndirectBufferAllocator.GetTotalFreeSpace(),
        DrawMetaDataStagingBufferSize = DrawMetaDataBufferAllocator.GetCapacity() - DrawMetaDataBufferAllocator.GetTotalFreeSpace();

    size_t StagingBufferSize = DrawMetaDataStagingBufferSize;
    if (VertexBufferNeedsDefragmentation) StagingBufferSize += VertexStagingBufferSize;
    if (IndexBufferNeedsDefragmentation) StagingBufferSize += IndexStagingBufferSize;
    if (IndirectBufferNeedsDefragmentation) StagingBufferSize += IndirectStagingBufferSize;

    if (!DrawMetaDataStagingBufferSize || !StagingBufferSize)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed model erasing operation, faulty staging buffer!");
        throw std::runtime_error("Faulty staging buffer!");
    }

    RENDERER_CORE::VirtualArenaAllocator StagingBufferAllocator(StagingBufferSize);
    RENDERER_CORE::PersistentBuffer StagingBuffer;
    uint8_t* StagingBufferPtr = nullptr;
    if (StagingBufferSize)
    {
        RENDERER_CORE::CreateStagingBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            StagingBufferSize,
            StagingBuffer.Buffer
        );

        StagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StagingBufferSize, 0);
        StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.MappedMemory);
    }
    if (!StagingBufferPtr)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Unable to proceed model erasing operation, failed staging buffer creation!");
        throw std::runtime_error("Failed staging buffer creation!");
    }

    std::vector<RENDERER_CORE::BufferCopyInfo> CopyInfos(4);
    //Redistribute the model matrixes.
    size_t ModelIterator = 0;
    uint32_t CurrentMesh = 0;

    DrawMetaDataBufferAllocator.Reset();
    for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
    {
        //Gather memory regions to defragment. 
        for (auto& MeshEntry : ModelEntryMetaData.MeshEntries)
        {
            //Local instance index iterator
            size_t InstanceIterator = 0;
            for (auto& [InstancePtr, AllocatedMemoryRegions] : ModelEntryMetaData.Instances)
            {
                DrawMetadata NewDrawMetaData{};
                NewDrawMetaData.MeshID = CurrentMesh;
                NewDrawMetaData.ModelMatrixIndex = AllocatedMemoryRegions[0].Offset / SizeOfMat4;

                RENDERER_CORE::MemoryRegion DrawMetaDataAllocatedRegion = DrawMetaDataBufferAllocator.Allocate(SizeOfDrawMetaData);
                RENDERER_CORE::MemoryRegion DrawMetaDataStagingAllocatedRegion = StagingBufferAllocator.Allocate(SizeOfDrawMetaData);
                AllocatedMemoryRegions[1] = DrawMetaDataAllocatedRegion;

                memcpy(StagingBufferPtr + DrawMetaDataStagingAllocatedRegion.Offset, &NewDrawMetaData, DrawMetaDataAllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = DrawMetaDataAllocatedRegion.Offset;
                CopyRegion.size = DrawMetaDataAllocatedRegion.Size;
                CopyRegion.srcOffset = DrawMetaDataStagingAllocatedRegion.Offset;
                CopyInfos[DRAWMETA_COPY].CopyRegions.push_back(CopyRegion);

                InstanceIterator++;
            }
 
            if (!ShouldDefragment) continue;

            if (VertexBufferNeedsDefragmentation) VertexAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[0]);
            if (IndexBufferNeedsDefragmentation) IndexAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[1]);
            if ((VertexBufferNeedsDefragmentation || IndexBufferNeedsDefragmentation || IndirectBufferNeedsDefragmentation)) IndirectCommandAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[2]);
        }
        ModelIterator += ModelEntryMetaData.MetaData.ReferenceCount;
    }

    if (ShouldDefragment || AreIndirectCommandsChanged)
    {
        //Defragmenting the buffer allocators
        if (VertexBufferNeedsDefragmentation) VertexBufferAllocator.Defragment(VertexAllocatedRegions);
        if (IndexBufferNeedsDefragmentation) IndexBufferAllocator.Defragment(IndexAllocatedRegions);
        if ((VertexBufferNeedsDefragmentation || IndexBufferNeedsDefragmentation || IndirectBufferNeedsDefragmentation)) IndirectBufferAllocator.Defragment(IndirectCommandAllocatedRegions);

        size_t DrawIndexIterator = 0;
        CurrentMesh = 0;
        //Produce copy informations
        for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
        {
            for (auto& MeshEntry : ModelEntryMetaData.MeshEntries)
            {
                auto& MeshEntryPtr = MeshEntry.MeshPtr;
                auto& MeshEntryMetaData = MeshEntry.MetaData;

                if (VertexBufferNeedsDefragmentation)
                {
                    const RENDERER_CORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[0];
                    const RENDERER_CORE::MemoryRegion& StagingBufferAllocatedRegion = StagingBufferAllocator.Allocate(AllocatedRegion.Size);
                    memcpy(StagingBufferPtr + StagingBufferAllocatedRegion.Offset, MeshEntryPtr->Vertices.data(), StagingBufferAllocatedRegion.Size);

                    VkBufferCopy CopyRegion{};
                    CopyRegion.dstOffset = AllocatedRegion.Offset;
                    CopyRegion.size = AllocatedRegion.Size;
                    CopyRegion.srcOffset = StagingBufferAllocatedRegion.Offset;
                    CopyInfos[0].CopyRegions.push_back(CopyRegion);

                    MeshEntryMetaData.DrawInfo.VertexOffset = AllocatedRegion.Offset / sizeof(Vertex3D);
                }
                if (IndexBufferNeedsDefragmentation)
                {
                    const RENDERER_CORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[1];
                    const RENDERER_CORE::MemoryRegion& StagingBufferAllocatedRegion = StagingBufferAllocator.Allocate(AllocatedRegion.Size);
                    memcpy(StagingBufferPtr + StagingBufferAllocatedRegion.Offset, MeshEntryPtr->Indices.data(), StagingBufferAllocatedRegion.Size);

                    VkBufferCopy CopyRegion{};
                    CopyRegion.dstOffset = AllocatedRegion.Offset;
                    CopyRegion.size = AllocatedRegion.Size;
                    CopyRegion.srcOffset = StagingBufferAllocatedRegion.Offset;
                    CopyInfos[1].CopyRegions.push_back(CopyRegion);

                    MeshEntryMetaData.DrawInfo.FirstIndex = AllocatedRegion.Offset / sizeof(uint32_t);
                }
                if (ModelEntryMetaData.MetaData.IsChanged || VertexBufferNeedsDefragmentation || IndexBufferNeedsDefragmentation || IndirectBufferNeedsDefragmentation)
                {
                    ModelEntryMetaData.MetaData.IsChanged = false;

                    ExtendedIndirectCommand NewIndirectCommand{};
                    NewIndirectCommand.IndexCount = MeshEntryMetaData.DrawInfo.IndexCount;
                    NewIndirectCommand.FirstIndex = MeshEntryMetaData.DrawInfo.FirstIndex;
                    NewIndirectCommand.InstanceCount = ModelEntryMetaData.MetaData.ReferenceCount;
                    NewIndirectCommand.VertexOffset = MeshEntryMetaData.DrawInfo.VertexOffset;
                    NewIndirectCommand.BoundingBox = MeshEntryPtr->BoundingBox;
                    NewIndirectCommand.FirstInstance = DrawIndexIterator;

                    const RENDERER_CORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[2];
                    const RENDERER_CORE::MemoryRegion& StagingBufferAllocatedRegion = StagingBufferAllocator.Allocate(AllocatedRegion.Size);
                    memcpy(StagingBufferPtr + StagingBufferAllocatedRegion.Offset, &NewIndirectCommand, StagingBufferAllocatedRegion.Size);

                    VkBufferCopy CopyRegion{};
                    CopyRegion.dstOffset = AllocatedRegion.Offset;
                    CopyRegion.size = AllocatedRegion.Size;
                    CopyRegion.srcOffset = StagingBufferAllocatedRegion.Offset;
                    CopyInfos[2].CopyRegions.push_back(CopyRegion);
                }
                CurrentMesh++;
                DrawIndexIterator += ModelEntryMetaData.MetaData.ReferenceCount;
            }
        }
    }
    CopyInfos[VERTEX_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[VERTEX_COPY].DestinationBuffer = VertexBuffer.Buffer.BufferObject;
    CopyInfos[INDEX_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[INDEX_COPY].DestinationBuffer = IndexBuffer.Buffer.BufferObject;
    CopyInfos[INDIRECT_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[INDIRECT_COPY].DestinationBuffer = IndirectBuffer.Buffer.BufferObject;
    CopyInfos[DRAWMETA_COPY].SourceBuffer = StagingBuffer.Buffer.BufferObject;
    CopyInfos[DRAWMETA_COPY].DestinationBuffer = DrawMetaDataBuffer.Buffer.BufferObject;

    //Execute copy operations 
    RENDERER_CORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    StagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    */
}


void SCENE::SceneMeshManager::UpdateTextureDescriptors(MeshTextureUpdateInfo Info)
{
    auto FrameIndex = Info.FrameIndex;
    auto& TextureImportManager = *Info.TextureImportManagerPtr;
    auto& DescriptorSets = Info.TargetDescriptorSets;

    auto& CurrentDescriptorSet = DescriptorSets[FrameIndex];

    TextureImportManager.UpdateDescriptors(FrameIndex);
    auto& CurrentFrameEntries = ModelEntries[FrameIndex];

    if (CurrentFrameEntries.MaterialUpdateList.empty()) return;

    auto& CurrentTextureIndexBuffer = PerformanceModeBuffers.TexturesIndexBuffers[FrameIndex];
    auto& TextureIndexBufferAllocator = CurrentTextureIndexBuffer.Allocator;
    auto Capacity = TextureIndexBufferAllocator.GetCapacity() - TextureIndexBufferAllocator.GetTotalFreeSpace();
    auto& TexturesIndexBufferReallocated = PerformanceModeBuffers.TexturesIndexBuffersReallocated[FrameIndex];

    uint32_t MaterialTextureTypeCount = static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE);
    uint32_t Inserted = 0;
    size_t SizeOfTextureIndices = MaterialTextureTypeCount * sizeof(int);
    
    RENDERER_CORE::BufferCopyInfo CopyInfo{};
    for (auto& ModelInstancePtr : CurrentFrameEntries.MaterialUpdateList)
    {
        auto& InstanceEntryIterator = CurrentFrameEntries.InstanceEntries.find(ModelInstancePtr->ResourceID);
        for (auto& [MeshGeometryHandle,MetaData] : InstanceEntryIterator->second.Materials)
        {
            //std::cout << "MetaData.TextureIndexMemoryRegion.Offset: " << MetaData.TextureIndexMemoryRegion.Offset << std::endl;
            for (uint32_t i = 0; i < MaterialTextureTypeCount; i++)
            {
                auto TextureIndex = MetaData.Material.GetTexture(static_cast<MaterialTextureType>(i));
                if (TextureIndex)
                {
                    auto Iterator = TextureImportManager.TextureDatas.find(TextureIndex);
                    if (Iterator == TextureImportManager.TextureDatas.end())
                    {
                        MetaData.TextureIndexes[i] = -1;
                        continue;
                    }
                    auto& TextureDataEntry = TextureImportManager.TextureDatas[TextureIndex];

                    MetaData.TextureIndexes[i] = TextureDataEntry.DescriptorSlots[FrameIndex];
                }
                else MetaData.TextureIndexes[i] = -1;
            }
        }
    }
     
    if (TexturesIndexBufferReallocated)
    {
        CurrentTextureIndexBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TextureIndexBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CurrentTextureIndexBuffer.Buffer
        );

        RENDERER_CORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
            CurrentTextureIndexBuffer.Buffer,
            TextureIndexBufferAllocator.GetCapacity(),
            0,
            CurrentDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );

        RENDERER_CORE::WriteDescriptorSets(
            RendererContext->DeviceContext.logicalDevice,
            { DrawMetaDataBufferWrite },
            {}
        );
    }

    RENDERER_CORE::PersistentBuffer TextureIndexStagingBuffer;
    size_t StagingBufferSize = TexturesIndexBufferReallocated ? Capacity :
                               CurrentFrameEntries.MaterialUpdateList.size() * SizeOfTextureIndices;
    RENDERER_CORE::VirtualArenaAllocator StagingBufferAllocator(StagingBufferSize);
    RENDERER_CORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        StagingBufferSize,
        TextureIndexStagingBuffer.Buffer
    );
    TextureIndexStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StagingBufferSize, 0);
    auto StagingBufferPtr = reinterpret_cast<uint8_t*>(TextureIndexStagingBuffer.MappedMemory);

    if (!StagingBufferPtr) return;

    if (TexturesIndexBufferReallocated)
    {
        for (auto& [Handle,InstanceEntry] : CurrentFrameEntries.InstanceEntries)
        {
            for (auto& [MeshGeometryHandle, MetaData] : InstanceEntry.Materials)
            {
                auto Region = StagingBufferAllocator.Allocate(SizeOfTextureIndices);
                memcpy(StagingBufferPtr + Region.Offset, MetaData.TextureIndexes.data(), Region.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MetaData.TextureIndexMemoryRegion.Offset;
                CopyRegion.size = Region.Size;
                CopyRegion.srcOffset = Region.Offset;
                CopyInfo.CopyRegions.push_back(CopyRegion);
            }
        }
    }
    else
    {
        for (auto& ModelInstancePtr : CurrentFrameEntries.MaterialUpdateList)
        {
            auto& InstanceEntryIterator = CurrentFrameEntries.InstanceEntries.find(ModelInstancePtr->ResourceID);
            for (auto& [MeshGeometryHandle, MetaData] : InstanceEntryIterator->second.Materials)
            {
                auto Region = StagingBufferAllocator.Allocate(SizeOfTextureIndices);
                memcpy(StagingBufferPtr + Region.Offset, MetaData.TextureIndexes.data(), Region.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MetaData.TextureIndexMemoryRegion.Offset;
                CopyRegion.size = Region.Size;
                CopyRegion.srcOffset = Region.Offset;
                CopyInfo.CopyRegions.push_back(CopyRegion);
            }
        }
    }
    CopyInfo.SourceBuffer = TextureIndexStagingBuffer.Buffer.BufferObject;
    CopyInfo.DestinationBuffer = CurrentTextureIndexBuffer.Buffer.BufferObject;
  
    RENDERER_CORE::CopyBuffer(
        { CopyInfo },
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    TextureIndexStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    TexturesIndexBufferReallocated = false;
    CurrentFrameEntries.MaterialUpdateList.clear();
}
