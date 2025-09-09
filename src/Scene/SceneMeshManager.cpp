#include "SceneMeshManager.hpp"

#include "Mesh.hpp"
#include "ModelInstance.hpp"
#include "../Renderer/RendererContext.hpp"
#include "MaterialManager.hpp"
#include "Scene.hpp"
#include "MeshManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include <chrono>

SCENE::SceneMeshManager::SceneMeshManager(MeshManager& MeshManager, RENDERER::RendererContext &RendererContext)
{
    Create(MeshManager,RendererContext);
}

void SCENE::SceneMeshManager::Create(MeshManager& MeshManager, RENDERER::RendererContext& RendererContext)
{
    this->RendererContext = &RendererContext;
    this->MeshManagerPtr = &MeshManager;
    SceneBuffers.Create();
}

void SCENE::SceneMeshManager::Destroy(VkDevice& LogicalDevice)
{
    SceneBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
}

//Update mesh transformation buffers based on the update list
void SCENE::SceneMeshManager::UpdateMeshTransformations(std::vector<ModelInstance*>& UpdateList, uint32_t CurrentFrame)
{
    if (UpdateList.empty()) return;

    auto& ModelMatrixBuffer = SceneBuffers.ModelMatricesBuffers[CurrentFrame].Buffer;
    uint8_t* Destination = reinterpret_cast<uint8_t*>(ModelMatrixBuffer.MappedMemory);
    auto& CurrentFrameInstanceEntries = Entries[CurrentFrame].InstanceEntries;

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

//Processes the input model instances and produces mesh and instance entries  
void ProcessAppendList(
    std::vector<SCENE::ModelInstance*> &AppendList,
    SCENE::EntryManager &CurrentFrameEntries,
    std::unordered_map<size_t, SCENE::GeometryEntry> &CurrentFrameGeometryEntries,
    RENDERER_CORE::VirtualArenaAllocator &ModelMatricesBufferAllocator,
    RENDERER_CORE::VirtualArenaAllocator &TexturesIndexBufferAllocator,
    RENDERER_CORE::VirtualArenaAllocator &IndirectBufferAllocator,
    size_t &EnabledMeshCount,
    size_t &IndirectBufferSize,
    size_t &InsertedMeshInstanceCount
)
{
    size_t SizeOfVertex = sizeof(SCENE::Vertex3D),
        SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4),
        SizeOfDrawMetaData = sizeof(SCENE::DrawMetadata),
        SizeOfIndirectCommand = sizeof(SCENE::ExtendedIndirectCommand);

    CurrentFrameEntries.MeshEntries.Reserve(CurrentFrameEntries.MeshEntries.Size() + (AppendList.size() * 5));
    CurrentFrameEntries.InstanceEntries.reserve(CurrentFrameEntries.InstanceEntries.size() + AppendList.size());
    //Process newly inserted meshes.
    for (auto& ModelInstance : AppendList)
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

        //Couldn't find an existing instance entry with the provided ID so a new entry is introduced 
        SCENE::InstanceEntry NewInstanceEntry{};
        NewInstanceEntry.TransformationMatrixMemoryRegion = ModelMatricesBufferAllocator.Suballocate(SizeOfMat4);
      
        NewInstanceEntry.Materials.reserve(ModelInstance->Source->Meshes.size());
        //Process and allocate the individual meshes 
        for (uint32_t i = 0; i < ModelInstance->Source->Meshes.size(); i++)
        {
            auto& Mesh = ModelInstance->Source->Meshes[i];

            auto& GeometryEntryIterator = CurrentFrameGeometryEntries.find(Mesh.GeometryID);
            if (GeometryEntryIterator == CurrentFrameGeometryEntries.end())
            {
                throw std::runtime_error("Attempt on linking non-existent mesh instance!");
            }

            SCENE::MaterialMetaData NewMaterialMetaData{};
            if (ModelInstance->Materials.size() > i)
            {
                NewMaterialMetaData.Material = ModelInstance->Materials[i];
            }
            else
            {
                NewMaterialMetaData.Material = GeometryEntryIterator->second.MeshMaterial;
            }

            NewMaterialMetaData.TextureIndexMemoryRegion =
                TexturesIndexBufferAllocator.Suballocate(static_cast<uint32_t>(SCENE::MATERIAL_TEXTURE_TYPE_META_DATA_SIZE) * sizeof(int));
            NewInstanceEntry.Materials[Mesh.GeometryID] = std::move(NewMaterialMetaData);

            auto MeshEntryIterator = CurrentFrameEntries.MeshEntries.Find(Mesh.GeometryID);
            bool DoesMeshAlreadyExist = MeshEntryIterator != nullptr;
            if (DoesMeshAlreadyExist)
            {
                MeshEntryIterator->second.IsChanged = true;
                MeshEntryIterator->second.ReferenceCount++;
                MeshEntryIterator->second.InstanceLinks.push_back({ ModelInstance->ResourceID, RENDERER_CORE::MemoryRegion() });
                continue;
            }
            
            //Fill data in the new mesh entry
            SCENE::MeshEntry NewMeshEntry{};
            NewMeshEntry.BoundingBox = GeometryEntryIterator->second.BoundingBox;
            NewMeshEntry.ReferenceCount++;
            NewMeshEntry.IsChanged = true;
            NewMeshEntry.InstanceLinks.push_back({ ModelInstance->ResourceID, RENDERER_CORE::MemoryRegion() });

            SCENE::DrawInfo MeshDrawInfo{};
            MeshDrawInfo.VertexOffset = GeometryEntryIterator->second.VertexRegion.Offset / SizeOfVertex;
            MeshDrawInfo.FirstIndex = GeometryEntryIterator->second.IndexRegion.Offset / SizeOfUint32;
            MeshDrawInfo.IndexCount = GeometryEntryIterator->second.IndexRegion.Size / SizeOfUint32;
            NewMeshEntry.Info = MeshDrawInfo;

            NewMeshEntry.IndirectBufferMemoryRegion = IndirectBufferAllocator.Suballocate(SizeOfIndirectCommand);
            NewMeshEntry.ResourceID = Mesh.GeometryID;

            CurrentFrameEntries.MeshEntries.Insert( Mesh.GeometryID,NewMeshEntry);
            IndirectBufferSize += SizeOfIndirectCommand;

            EnabledMeshCount++;
        }
        CurrentFrameEntries.InstanceEntries[ModelInstance->ResourceID] = NewInstanceEntry;
        //Inserted total instance per mesh count
        InsertedMeshInstanceCount += ModelInstance->Source->Meshes.size();
    }
}

//Allocates or reallocates required scene buffers and writes them in descriptors
void AllocateSceneBuffers(
    RENDERER::RendererContext* RendererContext,
    std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(SCENE::BUFFER_COPY_SLOT_SIZE)>& CurrentCopyInfos,
    RENDERER_CORE::BufferAllocator &IndirectBuffer,
    RENDERER_CORE::BufferAllocator &DrawMetaDataBuffer,
    RENDERER_CORE::PersistentBufferAllocator &ModelMatricesBuffer,
    VkDescriptorSet TargetDescriptorSet,
    bool IsIndirectBufferReallocated,
    bool IsModelMatrixesBufferReallocated,
    size_t InitialModelMatrixBufferCapacity,
    bool IsDrawMetaDataBufferReallocated,
    bool TexturesIndexBufferReallocated
)
{
    if (IsIndirectBufferReallocated || IsModelMatrixesBufferReallocated || IsDrawMetaDataBufferReallocated || TexturesIndexBufferReallocated)
    {
        std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> WriteInfos;
        WriteInfos.reserve(3);
        //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
        //Case where the indirect command buffer needs to be reallocated or allocated
        if (IsIndirectBufferReallocated)
        {
            SCENE::RecreateBuffer(
                RendererContext,
                IndirectBuffer.Allocator.GetCapacity(),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                IndirectBuffer.Buffer
            );
           
            RENDERER_CORE::DescriptorSetWriteBuffer IndirectBufferWrite(
                IndirectBuffer.Buffer,
                IndirectBuffer.Allocator.GetCapacity(),
                1,
                TargetDescriptorSet,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
            );
            WriteInfos.push_back(std::move(IndirectBufferWrite));
            CurrentCopyInfos[SCENE::INDIRECT_COPY].DestinationBuffer = IndirectBuffer.Buffer.BufferObject;
        }
        //Case where the draw model matrix buffer needs to be reallocated or allocated
        if (IsModelMatrixesBufferReallocated)
        {
            //In case there is existing data we gotta restore them back to the new buffer
            uint8_t* TempSpace = nullptr;
            if (InitialModelMatrixBufferCapacity)
            {
                TempSpace = new uint8_t[InitialModelMatrixBufferCapacity];
                memcpy(TempSpace,ModelMatricesBuffer.Buffer.MappedMemory, InitialModelMatrixBufferCapacity);
            }

            //Recreate the buffer
            SCENE::RecreateBuffer(
                RendererContext,
                ModelMatricesBuffer.Allocator.GetCapacity(),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                ModelMatricesBuffer.Buffer.Buffer
            );
            //Map it since it's a persistent buffer
            ModelMatricesBuffer.Buffer.Map(RendererContext->DeviceContext.logicalDevice, 0, ModelMatricesBuffer.Allocator.GetCapacity(), 0);

            //Copy the existing data into the new buffer from temp buffer
            if (TempSpace)
            {
                memcpy(ModelMatricesBuffer.Buffer.MappedMemory,TempSpace, InitialModelMatrixBufferCapacity);
                delete[] TempSpace;
            }
            //Update the descriptor
            RENDERER_CORE::DescriptorSetWriteBuffer ModelMatrixBufferWrite(
                ModelMatricesBuffer.Buffer.Buffer,
                ModelMatricesBuffer.Allocator.GetCapacity(),
                0,
                TargetDescriptorSet,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
            );
            WriteInfos.push_back(std::move(ModelMatrixBufferWrite));
        }
        //Case where the draw meta data buffer needs to be reallocated or allocated
        if (IsDrawMetaDataBufferReallocated)
        {
            SCENE::RecreateBuffer(
                RendererContext,
                DrawMetaDataBuffer.Allocator.GetCapacity(),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                DrawMetaDataBuffer.Buffer
            );
           
            RENDERER_CORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
                DrawMetaDataBuffer.Buffer,
                DrawMetaDataBuffer.Allocator.GetCapacity(),
                2,
                TargetDescriptorSet,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
            );

            WriteInfos.push_back(std::move(DrawMetaDataBufferWrite));
            CurrentCopyInfos[SCENE::DRAWMETA_COPY].DestinationBuffer = DrawMetaDataBuffer.Buffer.BufferObject;
        }
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, WriteInfos, {});
    }
}

void CreateAppendCopyInfos(
    std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(SCENE::BUFFER_COPY_SLOT_SIZE)>& CurrentCopyInfos,
    SCENE::EntryManager &CurrentFrameEntries,
    RENDERER_CORE::BufferAllocator& IndirectBuffer,
    RENDERER_CORE::BufferAllocator& DrawMetaDataBuffer,
    RENDERER_CORE::PersistentBufferAllocator& StagingBuffer,
    bool IsIndirectBufferReallocated,
    bool IsThereIndirectCopyInfos,
    bool IsThereDrawMetaCopyInfos
)
{

    size_t SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4),
        SizeOfDrawMetaData = sizeof(SCENE::DrawMetadata),
        SizeOfIndirectCommand = sizeof(SCENE::ExtendedIndirectCommand);

    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);
    if (!StagingBufferPtr) throw std::runtime_error("Unable to map the staging buffer! exitting...");

    DrawMetaDataBuffer.Allocator.Reset();
    size_t MeshIterator = 0;
    //Global draw index iterator
    size_t DrawIndexIterator = 0;
    //Create or update indirect commands 
    SCENE::ExtendedIndirectCommand NewIndirectCommand{};
    CurrentCopyInfos[SCENE::DRAWMETA_COPY].CopyRegions.clear();
    if (IsThereIndirectCopyInfos && IsIndirectBufferReallocated) CurrentCopyInfos[SCENE::INDIRECT_COPY].CopyRegions.clear();

    CurrentCopyInfos[SCENE::DRAWMETA_COPY].CopyRegions.reserve(CurrentFrameEntries.MeshEntries.Size() * 10);
    CurrentCopyInfos[SCENE::INDIRECT_COPY].CopyRegions.reserve(CurrentFrameEntries.MeshEntries.Size());

    for (auto& [Handle, MeshEntry] : CurrentFrameEntries.MeshEntries)
    {
        //Redistribute draw meta datas
        for (size_t i = 0; i < MeshEntry.InstanceLinks.size(); i++)
        {
            SCENE::InstanceMeshLink& InstanceLink = MeshEntry.InstanceLinks[i];

            auto& InstanceIterator = CurrentFrameEntries.InstanceEntries.find(InstanceLink.ResourceID);
            if (InstanceIterator == CurrentFrameEntries.InstanceEntries.end()) continue;

            auto& MaterialDataIterator = InstanceIterator->second.Materials.find(Handle);

            SCENE::DrawMetadata NewDrawMetaData{};
            NewDrawMetaData.MeshID = MaterialDataIterator->second.TextureIndexMemoryRegion.Offset / (static_cast<uint32_t>(SCENE::MATERIAL_TEXTURE_TYPE_META_DATA_SIZE) * sizeof(int));
            NewDrawMetaData.ModelMatrixIndex = InstanceIterator->second.TransformationMatrixMemoryRegion.Offset / SizeOfMat4;

            InstanceLink.DrawDataMemoryRegion = DrawMetaDataBuffer.Allocator.Suballocate(SizeOfDrawMetaData);
            if (!IsThereDrawMetaCopyInfos || !InstanceLink.StagingDrawDataMemoryRegion.Size)
            {
                InstanceLink.StagingDrawDataMemoryRegion = StagingBuffer.Allocator.Suballocate(SizeOfDrawMetaData);
            }

            memcpy(StagingBufferPtr + InstanceLink.StagingDrawDataMemoryRegion.Offset, &NewDrawMetaData, InstanceLink.StagingDrawDataMemoryRegion.Size);

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = InstanceLink.DrawDataMemoryRegion.Offset;
            CopyRegion.size = InstanceLink.DrawDataMemoryRegion.Size;
            CopyRegion.srcOffset = InstanceLink.StagingDrawDataMemoryRegion.Offset;
            CurrentCopyInfos[SCENE::DRAWMETA_COPY].CopyRegions.push_back(CopyRegion);
        }

        //Construct indirect commands
        if ((MeshEntry.FirstInstance != DrawIndexIterator) || MeshEntry.IsChanged || IsIndirectBufferReallocated)
        {
            MeshEntry.IsChanged = false;

            NewIndirectCommand.IndexCount = MeshEntry.Info.IndexCount;
            NewIndirectCommand.FirstIndex = MeshEntry.Info.FirstIndex;
            NewIndirectCommand.InstanceCount = MeshEntry.ReferenceCount;
            NewIndirectCommand.VertexOffset = MeshEntry.Info.VertexOffset;
            NewIndirectCommand.FirstInstance = DrawIndexIterator;
            //NewIndirectCommand.BoundingBox = MeshEntry.BoundingBox;
            
            //Should allocate a region from the staging buffer
            if (!IsThereIndirectCopyInfos || !MeshEntry.StagingIndirectBufferMemoryRegion.Size)
            {
                MeshEntry.StagingIndirectBufferMemoryRegion = StagingBuffer.Allocator.Suballocate(SizeOfIndirectCommand);
            }
            memcpy(StagingBufferPtr + MeshEntry.StagingIndirectBufferMemoryRegion.Offset, &NewIndirectCommand, MeshEntry.StagingIndirectBufferMemoryRegion.Size);

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = MeshEntry.IndirectBufferMemoryRegion.Offset;
            CopyRegion.size = MeshEntry.StagingIndirectBufferMemoryRegion.Size;
            CopyRegion.srcOffset = MeshEntry.StagingIndirectBufferMemoryRegion.Offset;
            CurrentCopyInfos[SCENE::INDIRECT_COPY].CopyRegions.push_back(CopyRegion);
        }
        if (MeshEntry.FirstInstance != DrawIndexIterator) MeshEntry.FirstInstance = DrawIndexIterator;

        DrawIndexIterator += MeshEntry.ReferenceCount;
        MeshIterator++;
    }
}

void SCENE::SceneMeshManager::ResetModels(uint32_t FrameIndex)
{
    //this->Entries[FrameIndex];x
}

void SCENE::SceneMeshManager::AppendModels(
    std::vector<ModelInstance*>& ModelInstances,
    const uint32_t& FrameIndex,
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& TargetDescriptorSets,
    PersistentStagingBuffer& StagingBuffer,
    std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
)
{
    auto Start = std::chrono::high_resolution_clock::now();
    this->MeshManagerPtr->UpdateGeometryEntries(FrameIndex);

    size_t& EnabledMeshCount = SceneBuffers.EnabledMeshCount[FrameIndex];

    auto& IndirectBuffer = SceneBuffers.IndirectBuffers[FrameIndex];
    auto& TexturesIndexBuffer = SceneBuffers.TexturesIndexBuffers[FrameIndex];

    auto& CullBuffers = SceneBuffers.CullBuffers;
    auto& CulledMetaDataBuffer = CullBuffers.CulledDrawMetaDataBuffer[FrameIndex];
    auto& CulledIndirectBuffer = CullBuffers.CulledIndirectBuffers[FrameIndex];
    auto& MeshVisibilityCountBuffer = CullBuffers.MeshVisibilityCountBuffers[FrameIndex];

    auto& DrawMetaDataBuffer = SceneBuffers.DrawMetaDataBuffer[FrameIndex];
    auto& ModelMatricesBuffer = SceneBuffers.ModelMatricesBuffers[FrameIndex];

    auto& IndirectBufferAllocator = IndirectBuffer.Allocator;
    auto& ModelMatricesBufferAllocator = ModelMatricesBuffer.Allocator;
    auto& DrawMetaDataBufferAllocator = DrawMetaDataBuffer.Allocator;
    auto& MeshVisibilityCountBufferAllocator = MeshVisibilityCountBuffer.Allocator;
    auto& TexturesIndexBufferAllocator = TexturesIndexBuffer.Allocator;

    auto& TexturesIndexBufferReallocated = SceneBuffers.TexturesIndexBuffersReallocated[FrameIndex];

    size_t IndirectBufferSize = 0;
    size_t SizeOfDrawMetaData = sizeof(DrawMetadata);

    //Initial buffer capacities 
    size_t  IndirectBufferCapacity = IndirectBufferAllocator.GetCapacity(),
            ModelMatricesBufferCapacity = ModelMatricesBufferAllocator.GetCapacity(),
            MeshVisibilityCountBufferCapacity = MeshVisibilityCountBufferAllocator.GetCapacity(),
            TexturesIndexBufferCapacity = TexturesIndexBufferAllocator.GetCapacity(),
            DrawMetaDataBufferCapacity = DrawMetaDataBufferAllocator.GetCapacity();

    size_t InsertedMeshInstanceCount = 0;

    auto& CurrentFrameEntries = Entries[FrameIndex];
    auto& CurrentFrameGeometryEntries = this->MeshManagerPtr->GeometryEntries[FrameIndex];

    bool IsThereDrawMetaCopyInfos = !CopyInfos[DRAWMETA_COPY].CopyRegions.empty();
    bool IsThereIndirectCopyInfos = !CopyInfos[INDIRECT_COPY].CopyRegions.empty();

    //Process newly inserted meshes.
    ProcessAppendList(
        ModelInstances, 
        CurrentFrameEntries, 
        CurrentFrameGeometryEntries, 
        ModelMatricesBufferAllocator, 
        TexturesIndexBufferAllocator, 
        IndirectBufferAllocator, 
        EnabledMeshCount, 
        IndirectBufferSize, 
        InsertedMeshInstanceCount
    );
    DrawMetaDataBufferAllocator.Suballocate(InsertedMeshInstanceCount * SizeOfDrawMetaData);

    //Check whether the allocator allocated extra virtual memory
    bool IsIndirectBufferReallocated = IndirectBufferCapacity < IndirectBufferAllocator.GetCapacity();
    bool IsModelMatrixesBufferReallocated = ModelMatricesBufferCapacity < ModelMatricesBufferAllocator.GetCapacity();
    bool IsMeshVisibilityCountBufferReallocated = MeshVisibilityCountBufferCapacity < MeshVisibilityCountBufferAllocator.GetCapacity();
    bool IsDrawMetaDataBufferReallocated = DrawMetaDataBufferCapacity < DrawMetaDataBufferAllocator.GetCapacity();

    TexturesIndexBufferReallocated = TexturesIndexBufferCapacity < TexturesIndexBufferAllocator.GetCapacity();

    //No more work to do. Return
    if (!InsertedMeshInstanceCount && !IsIndirectBufferReallocated && 
        !IsModelMatrixesBufferReallocated && !IsDrawMetaDataBufferReallocated && !TexturesIndexBufferReallocated)
    {
        return;
    }

    //Handle allocation or reallocation of the scene buffers
    //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
    AllocateSceneBuffers(
        RendererContext,
        CopyInfos,
        IndirectBuffer,
        DrawMetaDataBuffer, 
        ModelMatricesBuffer,
        TargetDescriptorSets[FrameIndex],
        IsIndirectBufferReallocated, 
        IsModelMatrixesBufferReallocated, 
        ModelMatricesBufferCapacity,
        IsDrawMetaDataBufferReallocated, 
        TexturesIndexBufferReallocated
    );
         
    size_t IndirectStagingBufferSize = IsIndirectBufferReallocated ? IndirectBufferAllocator.GetCapacity() : IndirectBufferSize;
    size_t DrawMetaDataBufferSize = IsThereDrawMetaCopyInfos ? (InsertedMeshInstanceCount * SizeOfDrawMetaData) : DrawMetaDataBufferAllocator.GetCapacity();
    size_t TotalStagingBufferSize = IndirectStagingBufferSize + DrawMetaDataBufferSize;
    //Nothing else to do. Return
    if (!TotalStagingBufferSize)
    {
        return;
    }

    //Handle the allocation or reallocation of the scene persisten staging buffer
    StagingBuffer.AllocateSceneStagingBuffer(CopyInfos, TotalStagingBufferSize, RendererContext);

    //Create or update the indirect commands and draw meta datas
    CreateAppendCopyInfos(
        CopyInfos,
        CurrentFrameEntries,
        IndirectBuffer,
        DrawMetaDataBuffer,
        StagingBuffer.StagingBuffer, 
        IsIndirectBufferReallocated,
        IsThereIndirectCopyInfos, 
        IsThereDrawMetaCopyInfos
    );
    std::cout << " It took : " << std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - Start).count() << std::endl;
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

//Updating texture index buffer which links materials with the actual texture descriptor slots 
void SCENE::SceneMeshManager::UpdateTextureDescriptors(MeshTextureUpdateInfo Info)
{
    auto FrameIndex = Info.FrameIndex;
    auto& TextureImportManager = *Info.TextureImportManagerPtr;
    auto& DescriptorSets = Info.TargetDescriptorSets;

    auto& CurrentDescriptorSet = DescriptorSets[FrameIndex];

    TextureImportManager.UpdateDescriptors(FrameIndex);
    auto& CurrentFrameEntries = Entries[FrameIndex];

    if (CurrentFrameEntries.MaterialUpdateList.empty()) return;

    auto& CurrentTextureIndexBuffer = SceneBuffers.TexturesIndexBuffers[FrameIndex];
    auto& TextureIndexBufferAllocator = CurrentTextureIndexBuffer.Allocator;
    auto Capacity = TextureIndexBufferAllocator.GetCapacity() - TextureIndexBufferAllocator.GetTotalFreeSpace();
    auto& TexturesIndexBufferReallocated = SceneBuffers.TexturesIndexBuffersReallocated[FrameIndex];
    auto& StagingBuffer = *Info.StagingBuffer;
    auto& CopyInfos = *Info.CopyInfos;

    bool IsThereTextureIndexCopyInfos = !CopyInfos[TEXTUREINDEX_COPY].CopyRegions.empty();

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
        CopyInfos[TEXTUREINDEX_COPY].DestinationBuffer = CurrentTextureIndexBuffer.Buffer.BufferObject;
    }

   // RENDERER_CORE::PersistentBuffer TextureIndexStagingBuffer;
    size_t StagingBufferSize = TexturesIndexBufferReallocated ? Capacity :
                               CurrentFrameEntries.MaterialUpdateList.size() * SizeOfTextureIndices;

    StagingBuffer.AllocateSceneStagingBuffer(CopyInfos, StagingBufferSize, RendererContext);
    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.StagingBuffer.Buffer.MappedMemory);
    if (!StagingBufferPtr) return;

    CopyInfos[TEXTUREINDEX_COPY].CopyRegions.reserve(CurrentFrameEntries.InstanceEntries.size() * 10);
    if (TexturesIndexBufferReallocated)
    {
        if(IsThereTextureIndexCopyInfos) CopyInfos[TEXTUREINDEX_COPY].CopyRegions.clear();
        for (auto& [Handle,InstanceEntry] : CurrentFrameEntries.InstanceEntries)
        {
            for (auto& [MeshGeometryHandle, MetaData] : InstanceEntry.Materials)
            {
                //auto Region = StagingBufferAllocator.Suballocate(SizeOfTextureIndices);
                if (!IsThereTextureIndexCopyInfos || !MetaData.StagingTextureIndexMemoryRegion.Size)
                {
                    MetaData.StagingTextureIndexMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfTextureIndices);
                }
                memcpy(StagingBufferPtr + MetaData.StagingTextureIndexMemoryRegion.Offset, MetaData.TextureIndexes.data(), MetaData.StagingTextureIndexMemoryRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MetaData.TextureIndexMemoryRegion.Offset;
                CopyRegion.size = MetaData.StagingTextureIndexMemoryRegion.Size;
                CopyRegion.srcOffset = MetaData.StagingTextureIndexMemoryRegion.Offset;
                CopyInfos[TEXTUREINDEX_COPY].CopyRegions.push_back(CopyRegion);
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
                //auto Region = StagingBufferAllocator.Suballocate(SizeOfTextureIndices);
                if (!IsThereTextureIndexCopyInfos || !MetaData.StagingTextureIndexMemoryRegion.Size)
                {
                    MetaData.StagingTextureIndexMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfTextureIndices);
                }
                memcpy(StagingBufferPtr + MetaData.StagingTextureIndexMemoryRegion.Offset, MetaData.TextureIndexes.data(), MetaData.StagingTextureIndexMemoryRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MetaData.TextureIndexMemoryRegion.Offset;
                CopyRegion.size = MetaData.StagingTextureIndexMemoryRegion.Size;
                CopyRegion.srcOffset = MetaData.StagingTextureIndexMemoryRegion.Offset;
                CopyInfos[TEXTUREINDEX_COPY].CopyRegions.push_back(CopyRegion);
            }
        }
    }
   
    TexturesIndexBufferReallocated = false;
    CurrentFrameEntries.MaterialUpdateList.clear();
}
