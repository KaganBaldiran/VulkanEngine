#include "SceneMeshManager.hpp"

#include "Mesh.hpp"
#include "ModelInstance.hpp"
#include "Scene.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Renderer/ResourceManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include <chrono>

SCENE::SceneMeshManager::SceneMeshManager(RENDERER::ResourceManager& ResourceManager, RENDERER::RendererContext &RendererContext,size_t BufferAllocationStep)
{
    Create(ResourceManager,RendererContext, BufferAllocationStep);
}

void SCENE::SceneMeshManager::Create(RENDERER::ResourceManager& ResourceManager, RENDERER::RendererContext& RendererContext, size_t BufferAllocationStep)
{
    this->RendererContext = &RendererContext;
    this->ResourceManagerPtr = &ResourceManager;
    Buffers.Create(BufferAllocationStep);
}

void SCENE::SceneMeshManager::Destroy(VkDevice& LogicalDevice)
{
    Buffers.Destroy(RendererContext->DeviceContext.LogicalDevice);
}

//Model matrix buffer update function for host visible case
void SCENE::SceneMeshManager::UpdateMeshTransformationsHostVisible(std::vector<ModelInstance*>& UpdateList, uint32_t CurrentFrame)
{
    if (UpdateList.empty()) return;

    auto& ModelMatrixBuffer = Buffers.ModelMatricesBuffers[CurrentFrame].Buffer;
    uint8_t* Destination = reinterpret_cast<uint8_t*>(ModelMatrixBuffer.MappedMemory);
    auto& CurrentFrameInstanceEntries = Entries[CurrentFrame].InstanceEntries;

    std::vector<VkMappedMemoryRange> MemoryRanges;
    MemoryRanges.reserve(UpdateList.size());
    for (auto& ModelInstance : UpdateList)
    {
        auto ModelInstanceEntryIterator = CurrentFrameInstanceEntries.find(ModelInstance->GetHandleID());
        if (!ModelInstanceEntryIterator) continue;

        //Copy onto the destination buffer
        auto& AllocatedMemoryRegion = ModelInstanceEntryIterator->second.TransformationMatrixMemoryRegion;
        SCENE::INTERNAL::ModelTransformMatrices TransformMatrices{};
        TransformMatrices.ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
        TransformMatrices.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(TransformMatrices.ModelMatrix)));

        memcpy(Destination + AllocatedMemoryRegion.Offset,
            &TransformMatrices,
            AllocatedMemoryRegion.Size
        );

        //Records mapped memory ranges to flush.
        //Speed difference between this method and COHERENT buffer is unclear.
        VkMappedMemoryRange MemoryRange{};
        MemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        MemoryRange.offset = AllocatedMemoryRegion.Offset;
        MemoryRange.memory = ModelMatrixBuffer.BufferMemory;
        MemoryRange.size = AllocatedMemoryRegion.Size;
        MemoryRanges.push_back(std::move(MemoryRange));
    }

    //Flush the updated regions to make them visible
    vkFlushMappedMemoryRanges(RendererContext->DeviceContext.LogicalDevice, static_cast<uint32_t>(MemoryRanges.size()), MemoryRanges.data());
}

//Model matrix buffer update function for device local case
void SCENE::SceneMeshManager::UpdateMeshTransformationsDeviceLocal(
    std::vector<ModelInstance*>& UpdateList, 
    uint32_t CurrentFrame
    //std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)>& CopyOperations,
    //SCENE::PersistentStagingBuffer& StagingBuffer
)
{
    auto& CurrentFrameEntries = Entries[CurrentFrame];
    if (UpdateList.empty() && !CurrentFrameEntries.TransformationMatrixReallocated) return;

    auto& ModelMatrixBuffer = Buffers.ModelMatricesBuffers[CurrentFrame].Buffer;
    auto& CurrentFrameInstanceEntries = CurrentFrameEntries.InstanceEntries;

    size_t SizeOfMat4 = sizeof(glm::mat4);
    size_t SizeOfTransformMatrices = sizeof(SCENE::INTERNAL::ModelTransformMatrices);

    //Makes sure current staging buffer capacity is enough to take what's coming.
    //size_t MatrixBufferSize = CurrentFrameEntries.TransformationMatrixReallocated ? (CurrentFrameInstanceEntries.size() * SizeOfTransformMatrices) : (UpdateList.size() * SizeOfTransformMatrices);
    //StagingBuffer.AllocateSceneStagingBuffer(MatrixBufferSize, RendererContext);
    //uint8_t* Destination = reinterpret_cast<uint8_t*>(StagingBuffer.StagingBuffer.Buffer.MappedMemory);

    std::vector<SCENE::INTERNAL::ModelTransformMatrices> AppendedTransformMatrices;
    std::vector<VkBufferCopy> TransformMatricesCopyInfos;

    //If the buffer is reallocated and data must be recovered, update the entries using the update list and rewrite them back into the buffer.
   // bool IsThereCopyInfo = !CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.empty();
    if (CurrentFrameEntries.TransformationMatrixReallocated)
    {
        //Updates the model matrixes if given.
        for (auto& ModelInstance : UpdateList)
        {
            auto ModelInstanceEntryIterator = CurrentFrameInstanceEntries.find(ModelInstance->GetHandleID());
            if (!ModelInstanceEntryIterator) continue;

            glm::mat4 ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
            ModelInstanceEntryIterator->second.TransformMatrices.ModelMatrix = ModelMatrix;
            ModelInstanceEntryIterator->second.TransformMatrices.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(ModelMatrix)));
        }

        //Refresh the copy regions since they will be overwritten anyways.
        //CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.clear();
       // CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.reserve(CurrentFrameInstanceEntries.size());
        //Iterate over the instance entries and contruct the required copy infos.
        //TO-DO Unordered map is slow to iterate over, might benefit from VectorMap instead.
        for (auto& [Handle,InstanceEntry] : CurrentFrameInstanceEntries)
        {
            auto& AllocatedMemoryRegion = InstanceEntry.TransformationMatrixMemoryRegion;
            
            /*
            auto& StagingAllocatedMemoryRegion = InstanceEntry.StagingTransformationMatrixMemoryRegion;
            if (!StagingAllocatedMemoryRegion.Size || !IsThereCopyInfo)
            {
                InstanceEntry.StagingTransformationMatrixMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfTransformMatrices);
            }
           

            memcpy(Destination + StagingAllocatedMemoryRegion.Offset,
                &InstanceEntry.TransformMatrices,
                StagingAllocatedMemoryRegion.Size
            );
             */

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = AllocatedMemoryRegion.Offset;
            CopyRegion.size = SizeOfTransformMatrices;
            CopyRegion.srcOffset = AppendedTransformMatrices.size() * SizeOfTransformMatrices;
            //CopyRegion.size = StagingAllocatedMemoryRegion.Size;
            //CopyRegion.srcOffset = StagingAllocatedMemoryRegion.Offset;
            //CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.push_back(CopyRegion);
            TransformMatricesCopyInfos.push_back(std::move(CopyRegion));
            AppendedTransformMatrices.push_back(InstanceEntry.TransformMatrices);
        }
        CurrentFrameEntries.TransformationMatrixReallocated = false;
    }
    else
    {
        //Buffer isn't reallocated so update list suffice.
        //Constracts copy regions from the update list.
        //CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.reserve(UpdateList.size());
        for (auto& ModelInstance : UpdateList)
        {
            auto ModelInstanceEntryIterator = CurrentFrameInstanceEntries.find(ModelInstance->GetHandleID());
            if (!ModelInstanceEntryIterator) continue;

            auto& AllocatedMemoryRegion = ModelInstanceEntryIterator->second.TransformationMatrixMemoryRegion;
            /*
            auto& StagingAllocatedMemoryRegion = ModelInstanceEntryIterator->second.StagingTransformationMatrixMemoryRegion;
            if (!StagingAllocatedMemoryRegion.Size || !IsThereCopyInfo)
            {
                StagingAllocatedMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfTransformMatrices);
            }
            */

            SCENE::INTERNAL::ModelTransformMatrices TransformMatrices{};
            TransformMatrices.ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
            TransformMatrices.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(TransformMatrices.ModelMatrix)));
            //glm::mat4 ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
            /*
            memcpy(Destination + StagingAllocatedMemoryRegion.Offset,
                &TransformMatrices,
                StagingAllocatedMemoryRegion.Size
            );
            */

            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = AllocatedMemoryRegion.Offset;
            CopyRegion.size = SizeOfTransformMatrices;
            CopyRegion.srcOffset = AppendedTransformMatrices.size() * SizeOfTransformMatrices;
            //CopyRegion.size = StagingAllocatedMemoryRegion.Size;
            //CopyRegion.srcOffset = StagingAllocatedMemoryRegion.Offset;

            ModelInstanceEntryIterator->second.TransformMatrices = TransformMatrices;
            AppendedTransformMatrices.push_back(std::move(TransformMatrices));
            TransformMatricesCopyInfos.push_back(std::move(CopyRegion));
            //CopyOperations[TRANSFORMATION_MATRIX_COPY]->CopyRegions.push_back(CopyRegion);

        }
    }

    if (!AppendedTransformMatrices.empty())
    {
        std::shared_ptr<RENDERER::DataBlock> TransformMatricesDataBlock = std::make_shared<RENDERER::DataBlock>();
        TransformMatricesDataBlock->DataPtr = reinterpret_cast<uint8_t*>(AppendedTransformMatrices.data());
        TransformMatricesDataBlock->SizeInBytes = AppendedTransformMatrices.size() * SizeOfTransformMatrices;
        TransformMatricesDataBlock->Deleter = [LocalVector = std::move(AppendedTransformMatrices)]() {};

        ResourceManagerPtr->RequestCopyOperation(
            TransformMatricesCopyInfos,
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &ModelMatrixBuffer,
            TransformMatricesDataBlock,
            3,
            RENDERER::COPY_OPERATION_FLAG_ATOMIC
        );
    }
}

//Processes the input model instances and produces mesh and instance entries  
void ProcessAppendList(
    std::vector<SCENE::ModelInstance*>& AppendList,
    std::vector<SCENE::ModelInstance*>& MaterialUpdateList,
    SCENE::INTERNAL::EntryManager& CurrentFrameEntries,
    std::unordered_map<size_t, RENDERER::GeometryEntry>& CurrentFrameGeometryEntries,
    RENDERER_CORE::VirtualArenaAllocator& ModelMatricesBufferAllocator,
    RENDERER_CORE::VirtualArenaAllocator& TexturesIndexBufferAllocator,
    RENDERER_CORE::VirtualArenaAllocator& IndirectBufferAllocator,
    size_t& EnabledMeshCount,
    size_t& IndirectBufferSize,
    size_t& InsertedMeshInstanceCount
)
{
    size_t SizeOfVertex = sizeof(SCENE::Vertex3D),
        SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4),
        SizeOfTransformMatrices = sizeof(SCENE::INTERNAL::ModelTransformMatrices),
        SizeOfDrawMetaData = sizeof(SCENE::INTERNAL::DrawMetadata),
        SizeOfMaterialData = sizeof(SCENE::INTERNAL::MaterialData),
        SizeOfIndirectCommand = sizeof(SCENE::ExtendedIndirectCommand);

    CurrentFrameEntries.MeshEntries.reserve(CurrentFrameEntries.MeshEntries.size() + (AppendList.size() * 5));
    CurrentFrameEntries.InstanceEntries.reserve(CurrentFrameEntries.InstanceEntries.size() + AppendList.size());
    //Process newly inserted meshes.
    for (auto& ModelInstance : AppendList)
    {
        if (!ModelInstance || !ModelInstance->Source) continue;
        if (ModelInstance->Materials.size() > ModelInstance->Source->Meshes.size())
        {
            throw std::runtime_error("Instance material count exceeds target mesh count!");
        }

        MaterialUpdateList.push_back(ModelInstance);
        glm::mat4 ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
        glm::mat4 NormalMatrix = glm::transpose(glm::inverse(glm::mat3(ModelMatrix)));
        //Handle already existing instance case
        auto InstanceIterator = CurrentFrameEntries.InstanceEntries.find(ModelInstance->GetHandleID());
        if (InstanceIterator)
        {
            InstanceIterator->second.TransformMatrices.ModelMatrix = ModelMatrix;
            InstanceIterator->second.TransformMatrices.NormalMatrix = NormalMatrix;
            continue;
        }
        //Couldn't find an existing instance entry with the provided ID so a new entry is introduced 
        SCENE::INTERNAL::InstanceEntry NewInstanceEntry{};
        NewInstanceEntry.TransformationMatrixMemoryRegion = ModelMatricesBufferAllocator.Suballocate(SizeOfTransformMatrices);
        NewInstanceEntry.TransformMatrices.ModelMatrix = ModelMatrix;
        NewInstanceEntry.TransformMatrices.NormalMatrix = NormalMatrix;
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
           
            //Select the correct material
            SCENE::INTERNAL::MaterialMetaData NewMaterialMetaData{};
            NewMaterialMetaData.TextureIndexMemoryRegion =
                TexturesIndexBufferAllocator.Suballocate(SizeOfMaterialData);
            NewInstanceEntry.Materials[Mesh.GeometryID] = std::move(NewMaterialMetaData);

            auto MeshEntryIterator = CurrentFrameEntries.MeshEntries.find(Mesh.GeometryID);
            bool DoesMeshAlreadyExist = MeshEntryIterator != nullptr;
            if (DoesMeshAlreadyExist)
            {
                MeshEntryIterator->second.IsChanged = true;
                MeshEntryIterator->second.ReferenceCount++;
                MeshEntryIterator->second.InstanceLinks.push_back({ ModelInstance->GetHandleID(), RENDERER_CORE::MemoryRegion() });
                continue;
            }
            
            //Fill data in the new mesh entry
            SCENE::INTERNAL::MeshEntry NewMeshEntry{};
            NewMeshEntry.BoundingBox = GeometryEntryIterator->second.BoundingBox;
            NewMeshEntry.ReferenceCount++;
            NewMeshEntry.IsChanged = true;
            NewMeshEntry.InstanceLinks.push_back({ ModelInstance->GetHandleID(), RENDERER_CORE::MemoryRegion() });

            SCENE::DrawInfo MeshDrawInfo{};
            MeshDrawInfo.VertexOffset = GeometryEntryIterator->second.VertexRegion.Offset / SizeOfVertex;
            MeshDrawInfo.FirstIndex = GeometryEntryIterator->second.IndexRegion.Offset / SizeOfUint32;
            MeshDrawInfo.IndexCount = GeometryEntryIterator->second.IndexRegion.Size / SizeOfUint32;
            NewMeshEntry.Info = MeshDrawInfo;
            NewMeshEntry.PageIndex = GeometryEntryIterator->second.PageIndex;

            //NewMeshEntry.IndirectBufferMemoryRegion = IndirectBufferAllocator.Suballocate(SizeOfIndirectCommand);
            NewMeshEntry.ResourceID = Mesh.GeometryID;

            CurrentFrameEntries.MeshEntries.insert( Mesh.GeometryID,NewMeshEntry);
            IndirectBufferSize += SizeOfIndirectCommand;
            //EnabledMeshCount++;
        }
        CurrentFrameEntries.InstanceEntries.insert(ModelInstance->GetHandleID(), NewInstanceEntry);
        //Inserted total instance per mesh count
        InsertedMeshInstanceCount += ModelInstance->Source->Meshes.size();
        EnabledMeshCount += ModelInstance->Source->Meshes.size();
    }
}

//Allocates or reallocates required scene buffers and writes them in descriptors
void AllocateSceneBuffers(
    RENDERER::RendererContext* RendererContext,
   // std::array<RENDERER::CopyOperationEntry*, static_cast<int>(SCENE::BUFFER_COPY_SLOT_SIZE)>& CopyOperations,
    RENDERER_CORE::BufferAllocator &IndirectBuffer,
    RENDERER_CORE::BufferAllocator &DrawMetaDataBuffer,
    RENDERER_CORE::BufferAllocator &ModelMatricesBuffer,
    RENDERER_CORE::BufferAllocator &VisibilityBuffer,
    SCENE::SceneOptions SceneOptions,
    VkDescriptorSet TargetDescriptorSet,
    bool IsIndirectBufferReallocated,
    size_t IndirectBufferSize,
    bool IsModelMatrixesBufferReallocated,
    size_t InitialModelMatrixBufferCapacity,
    bool IsDrawMetaDataBufferReallocated,
    bool TexturesIndexBufferReallocated,
    bool IsVisibilityBufferReallocated
)
{
    if (IsIndirectBufferReallocated || IsModelMatrixesBufferReallocated ||
        IsDrawMetaDataBufferReallocated || TexturesIndexBufferReallocated || 
        IsVisibilityBufferReallocated)
    {
        std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> WriteInfos;
        WriteInfos.reserve(3);
        //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
        //Case where the indirect command buffer needs to be reallocated or allocated
        if (IsIndirectBufferReallocated)
        {
            IndirectBuffer.Allocator.Allocate((IndirectBuffer.Allocator.GetTotalFreeSpace() + IndirectBufferSize) - IndirectBuffer.Allocator.GetCapacity());
            RENDERER::RecreateBuffer(
                RendererContext,
                IndirectBuffer.Allocator.GetCapacity(),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
            //CopyOperations[SCENE::INDIRECT_COPY]->DestinationBuffer = &IndirectBuffer.Buffer;
        }
        //Case where the draw model matrix buffer needs to be reallocated or allocated
        //TO-DO could be moved into the actual matrix update function however effects are not quite certain. 
        //It might undermine the performance.
        if (IsModelMatrixesBufferReallocated)
        {
            //Allocate discrete types of memory depending on the given upload mode
            switch (SceneOptions.UploadMode)
            {
            //Host visible buffer mode, in case the buffer is already occupied with data. 
            //Copy them back into the new buffer using a heap allocated temporary buffer
            case SCENE::SCENE_DYNAMIC_UPLOAD_MODE_HOST_VISIBLE:
            {
                //In case there is existing data we gotta restore them back to the new buffer
                uint8_t* TempSpace = nullptr;
                if (InitialModelMatrixBufferCapacity)
                {
                    TempSpace = new uint8_t[InitialModelMatrixBufferCapacity];
                    memcpy(TempSpace, ModelMatricesBuffer.Buffer.MappedMemory, InitialModelMatrixBufferCapacity);
                }

                //Recreate the buffer
                RENDERER::RecreateBuffer(
                    RendererContext,
                    ModelMatricesBuffer.Allocator.GetCapacity(),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    ModelMatricesBuffer.Buffer
                );
                //Map it since it's a persistent buffer
                RENDERER_CORE::MapBuffer(ModelMatricesBuffer.Buffer, RendererContext->DeviceContext.LogicalDevice, 
                                                                0, ModelMatricesBuffer.Allocator.GetCapacity(), 0);
                //ModelMatricesBuffer.Buffer.Map(RendererContext->DeviceContext.LogicalDevice, 0, ModelMatricesBuffer.Allocator.GetCapacity(), 0);

                //Copy the existing data into the new buffer from temp buffer
                if (TempSpace)
                {
                    memcpy(ModelMatricesBuffer.Buffer.MappedMemory, TempSpace, InitialModelMatrixBufferCapacity);
                    delete[] TempSpace;
                }
                break;
            }
            //Device local case
            //Instead of copying the previous data here, we flag the recreation to the actual matrix update function
            case SCENE::SCENE_DYNAMIC_UPLOAD_MODE_DEVICE_LOCAL:
            {
                RENDERER::RecreateBuffer(
                    RendererContext,
                    ModelMatricesBuffer.Allocator.GetCapacity(),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    ModelMatricesBuffer.Buffer
                );
                //CopyOperations[SCENE::TRANSFORMATION_MATRIX_COPY]->DestinationBuffer = &ModelMatricesBuffer.Buffer;
                break;
            }
            default:
                break;
            }
            //Update the descriptor
            RENDERER_CORE::DescriptorSetWriteBuffer ModelMatrixBufferWrite(
                ModelMatricesBuffer.Buffer,
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
            RENDERER::RecreateBuffer(
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
            //CopyOperations[SCENE::DRAWMETA_COPY]->DestinationBuffer = &DrawMetaDataBuffer.Buffer;
        }
        if (IsVisibilityBufferReallocated)
        {
            RENDERER::RecreateBuffer(
                RendererContext,
                VisibilityBuffer.Allocator.GetCapacity(),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VisibilityBuffer.Buffer
            );

            RENDERER_CORE::DescriptorSetWriteBuffer VisibilityBufferWrite(
                VisibilityBuffer.Buffer,
                VisibilityBuffer.Allocator.GetCapacity(),
                3,
                TargetDescriptorSet,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
            );

            WriteInfos.push_back(std::move(VisibilityBufferWrite));
        }
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.LogicalDevice, WriteInfos, {});
    }
}

void CreateAppendCopyInfos(
    SCENE::INTERNAL::EntryManager &CurrentFrameEntries,
    RENDERER_CORE::BufferAllocator& IndirectBuffer,
    RENDERER_CORE::BufferAllocator& DrawMetaDataBuffer,
    std::vector<SCENE::INTERNAL::PageMeshCountEntry>& CurrentPageMeshCounts,
    size_t PageCount,
    bool IsIndirectBufferReallocated,
    RENDERER::ResourceManager *ResourceManagerPtr
)
{
    //Pointers to copy infos.
    //RENDERER::CopyOperationEntry* DrawMetaCopyInfo = CopyOperations[SCENE::DRAWMETA_COPY];
    //RENDERER::CopyOperationEntry* IndirectCopyInfo = CopyOperations[SCENE::INDIRECT_COPY];
    size_t SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4),
        SizeOfTransformMatrices= sizeof(SCENE::INTERNAL::ModelTransformMatrices),
        SizeOfDrawMetaData = sizeof(SCENE::INTERNAL::DrawMetadata),
        SizeOfIndirectCommand = sizeof(SCENE::ExtendedIndirectCommand);

   // uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);
    //if (!StagingBufferPtr) throw std::runtime_error("Unable to map the staging buffer! exitting...");

    size_t MeshIterator = 0;
    //Global draw index iterator
    size_t DrawIndexIterator = 0;
    //Create or update indirect commands 
   // if (IsThereIndirectCopyInfos && IsIndirectBufferReallocated) IndirectCopyInfo.CopyRegions.clear();
    //if (IsThereIndirectCopyInfos) IndirectCopyInfo->CopyRegions.clear();
    IndirectBuffer.Allocator.Reset(IndirectBuffer.Allocator.GetCapacity());
    DrawMetaDataBuffer.Allocator.Reset(DrawMetaDataBuffer.Allocator.GetCapacity());
    //DrawMetaCopyInfo->CopyRegions.clear();

    //Clear the page mesh counts to avoid accumulation over the passes. 
    CurrentPageMeshCounts.clear();
    CurrentPageMeshCounts.resize(PageCount);

    std::vector<SCENE::ExtendedIndirectCommand> AppendedIndirectCommands;
    std::vector<VkBufferCopy> IndirectCommandCopyInfos;

    std::vector<SCENE::INTERNAL::DrawMetadata> AppendedDrawMetadatas;
    std::vector<VkBufferCopy> DrawMetadataCopyInfos;
    //DrawMetaCopyInfo->CopyRegions.reserve(CurrentFrameEntries.MeshEntries.size() * 10);
    //IndirectCopyInfo->CopyRegions.reserve(CurrentFrameEntries.MeshEntries.size());

    CurrentFrameEntries.MeshEntries.sort([](auto& First, auto& Second) {
        if (First.second.PageIndex == Second.second.PageIndex)
        {
            return First.second.Info.FirstIndex < Second.second.Info.FirstIndex;
        }
        return First.second.PageIndex < Second.second.PageIndex;
    });
    for (auto& [Handle, MeshEntry] : CurrentFrameEntries.MeshEntries)
    {
        auto& PageMeshCountData = CurrentPageMeshCounts[MeshEntry.PageIndex];
        MeshEntry.IndirectBufferMemoryRegion = IndirectBuffer.Allocator.Suballocate(SizeOfIndirectCommand, 1, false);

        //Redistribute draw meta datas
        for (size_t i = 0; i < MeshEntry.InstanceLinks.size(); i++)
        {
            SCENE::INTERNAL::InstanceMeshLink& InstanceLink = MeshEntry.InstanceLinks[i];

            auto InstanceIterator = CurrentFrameEntries.InstanceEntries.find(InstanceLink.ResourceID);
            if (!InstanceIterator) continue;

            auto& MaterialDataIterator = InstanceIterator->second.Materials.find(Handle);

            SCENE::INTERNAL::DrawMetadata NewDrawMetaData{};
            NewDrawMetaData.MaterialID = MaterialDataIterator->second.TextureIndexMemoryRegion.Offset / sizeof(SCENE::INTERNAL::MaterialData);
            NewDrawMetaData.MeshID = MeshEntry.IndirectBufferMemoryRegion.Offset / SizeOfIndirectCommand;
            NewDrawMetaData.ModelMatrixIndex = InstanceIterator->second.TransformationMatrixMemoryRegion.Offset / SizeOfTransformMatrices;
            //NewDrawMetaData.MeshID = MeshIterator;

            InstanceLink.DrawDataMemoryRegion = DrawMetaDataBuffer.Allocator.Suballocate(SizeOfDrawMetaData);
            /*
            if (!IsThereDrawMetaCopyInfos || !InstanceLink.StagingDrawDataMemoryRegion.Size)
            {
                InstanceLink.StagingDrawDataMemoryRegion = StagingBuffer.Allocator.Suballocate(SizeOfDrawMetaData, 1, false);
            }

            memcpy(StagingBufferPtr + InstanceLink.StagingDrawDataMemoryRegion.Offset, &NewDrawMetaData, InstanceLink.StagingDrawDataMemoryRegion.Size);
            */


            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = InstanceLink.DrawDataMemoryRegion.Offset;
            CopyRegion.size = SizeOfDrawMetaData;
            CopyRegion.srcOffset = AppendedDrawMetadatas.size() * SizeOfDrawMetaData;
            //CopyRegion.size = InstanceLink.DrawDataMemoryRegion.Size;
            //CopyRegion.srcOffset = InstanceLink.StagingDrawDataMemoryRegion.Offset;
            //DrawMetaCopyInfo->CopyRegions.push_back(std::move(CopyRegion));
            AppendedDrawMetadatas.push_back(std::move(NewDrawMetaData));
            DrawMetadataCopyInfos.push_back(std::move(CopyRegion));
        }

        //Construct indirect commands
        if ((MeshEntry.FirstInstance != DrawIndexIterator) || MeshEntry.IsChanged || IsIndirectBufferReallocated)
        {
            MeshEntry.IsChanged = false;

            SCENE::ExtendedIndirectCommand NewIndirectCommand{};
            NewIndirectCommand.IndexCount = MeshEntry.Info.IndexCount;
            NewIndirectCommand.FirstIndex = MeshEntry.Info.FirstIndex;
            NewIndirectCommand.InstanceCount = 0;
            NewIndirectCommand.VertexOffset = MeshEntry.Info.VertexOffset;
            NewIndirectCommand.FirstInstance = DrawIndexIterator;
            NewIndirectCommand.BoundingBox = MeshEntry.BoundingBox;
            //NewIndirectCommand.FirstInstance = PageMeshCountData.InstanceCount;
            //NewIndirectCommand.InstanceCount = MeshEntry.ReferenceCount;
            
            /*
            //Should allocate a region from the staging buffer
            if (!IsThereIndirectCopyInfos || !MeshEntry.StagingIndirectBufferMemoryRegion.Size)
            {
                MeshEntry.StagingIndirectBufferMemoryRegion = StagingBuffer.Allocator.Suballocate(SizeOfIndirectCommand,1,false);
            }
            memcpy(StagingBufferPtr + MeshEntry.StagingIndirectBufferMemoryRegion.Offset, &NewIndirectCommand, MeshEntry.StagingIndirectBufferMemoryRegion.Size);
            */


            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = MeshEntry.IndirectBufferMemoryRegion.Offset;
            CopyRegion.size = SizeOfIndirectCommand;
            CopyRegion.srcOffset = AppendedIndirectCommands.size() * SizeOfIndirectCommand;
            //CopyRegion.size = MeshEntry.StagingIndirectBufferMemoryRegion.Size;
            //CopyRegion.srcOffset = MeshEntry.StagingIndirectBufferMemoryRegion.Offset;

            AppendedIndirectCommands.push_back(std::move(NewIndirectCommand));
            IndirectCommandCopyInfos.push_back(std::move(CopyRegion));
            //IndirectCopyInfo->CopyRegions.push_back(std::move(CopyRegion));
        }
        if (MeshEntry.FirstInstance != DrawIndexIterator) MeshEntry.FirstInstance = DrawIndexIterator;

        PageMeshCountData.MeshCount++;
        PageMeshCountData.InstanceCount += MeshEntry.ReferenceCount;

        DrawIndexIterator += MeshEntry.ReferenceCount;
        MeshIterator++;
    }

    if (!AppendedIndirectCommands.empty())
    {
        std::shared_ptr<RENDERER::DataBlock> IndirectDataBlock = std::make_shared<RENDERER::DataBlock>();
        IndirectDataBlock->DataPtr = reinterpret_cast<uint8_t*>(AppendedIndirectCommands.data());
        IndirectDataBlock->SizeInBytes = AppendedIndirectCommands.size() * SizeOfIndirectCommand;
        IndirectDataBlock->Deleter = [LocalVector = std::move(AppendedIndirectCommands)]() {};

        ResourceManagerPtr->RequestCopyOperation(
            IndirectCommandCopyInfos,
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &IndirectBuffer.Buffer,
            IndirectDataBlock,
            4,
            RENDERER::COPY_OPERATION_FLAG_ATOMIC,
            ResourceManagerPtr->MeshManager.GeometryBufferPageCopyTokens.data(),
            ResourceManagerPtr->MeshManager.GeometryBufferPageCopyTokens.size()
        );
    }

    if (!AppendedDrawMetadatas.empty())
    {
        std::shared_ptr<RENDERER::DataBlock> DrawMetadataBlock = std::make_shared<RENDERER::DataBlock>();
        DrawMetadataBlock->DataPtr = reinterpret_cast<uint8_t*>(AppendedDrawMetadatas.data());
        DrawMetadataBlock->SizeInBytes = AppendedDrawMetadatas.size() * SizeOfDrawMetaData;
        DrawMetadataBlock->Deleter = [LocalVector = std::move(AppendedDrawMetadatas)]() {};

        ResourceManagerPtr->RequestCopyOperation(
            DrawMetadataCopyInfos,
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &DrawMetaDataBuffer.Buffer,
            DrawMetadataBlock,
            3,
            RENDERER::COPY_OPERATION_FLAG_ATOMIC,
            ResourceManagerPtr->MeshManager.GeometryBufferPageCopyTokens.data(),
            ResourceManagerPtr->MeshManager.GeometryBufferPageCopyTokens.size()
        );
    }
}

void SCENE::SceneMeshManager::ResetModels(uint32_t FrameIndex)
{
    //this->Entries[FrameIndex];x
}

void SCENE::SceneMeshManager::AppendModels(
    std::vector<ModelInstance*>& ModelInstances,
    std::vector<ModelInstance*>& MaterialUpdateList,
    const uint32_t& FrameIndex,
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& TargetDescriptorSets,
    PersistentStagingBuffer& StagingBuffer,
    SCENE::SceneOptions SceneOptions
   // std::array<RENDERER::CopyOperationEntry*, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
)
{
    auto Start = std::chrono::high_resolution_clock::now();
    //this->ResourceManagerPtr->MeshManager.UpdateGeometryEntries();

    size_t& EnabledMeshCount = Buffers.EnabledMeshCount[FrameIndex];

    auto& IndirectBuffer = Buffers.IndirectBuffers[FrameIndex];
    auto& TexturesIndexBuffer = Buffers.TexturesIndexBuffers[FrameIndex];
    auto& DrawMetaDataBuffer = Buffers.DrawMetaDataBuffer[FrameIndex];
    auto& ModelMatricesBuffer = Buffers.ModelMatricesBuffers[FrameIndex];
    auto& VisibilityIndexBuffer = Buffers.CullBuffers.VisibilityIndexBuffers[FrameIndex];

    auto& IndirectBufferAllocator = IndirectBuffer.Allocator;
    auto& ModelMatricesBufferAllocator = ModelMatricesBuffer.Allocator;
    auto& DrawMetaDataBufferAllocator = DrawMetaDataBuffer.Allocator;
    auto& TexturesIndexBufferAllocator = TexturesIndexBuffer.Allocator;

    std::vector<SCENE::INTERNAL::PageMeshCountEntry>& CurrentPageMeshCounts = PageMeshCounts[FrameIndex];
    size_t GeometryBufferPageCount = ResourceManagerPtr->MeshManager.GeometryBufferPages.size();

    auto& TexturesIndexBufferReallocated = Buffers.TexturesIndexBuffersReallocated[FrameIndex];

    size_t IndirectAppendedBufferSize = 0;
    size_t SizeOfDrawMetaData = sizeof(INTERNAL::DrawMetadata);

    //Initial buffer capacities 
    size_t  IndirectBufferCapacity = IndirectBufferAllocator.GetCapacity(),
            ModelMatricesBufferCapacity = ModelMatricesBufferAllocator.GetCapacity(),
            TexturesIndexBufferCapacity = TexturesIndexBufferAllocator.GetCapacity(),
            DrawMetaDataBufferCapacity = DrawMetaDataBufferAllocator.GetCapacity(),
            VisibilityIndexBufferCapacity = VisibilityIndexBuffer.Allocator.GetCapacity();

    size_t InsertedMeshInstanceCount = 0;

    auto& CurrentFrameEntries = Entries[FrameIndex];
    auto& CurrentFrameGeometryEntries = this->ResourceManagerPtr->MeshManager.GeometryEntries;

    //bool IsThereDrawMetaCopyInfos = !CopyInfos[DRAWMETA_COPY]->CopyRegions.empty();
    //bool IsThereIndirectCopyInfos = !CopyInfos[INDIRECT_COPY]->CopyRegions.empty();

    //Process newly inserted meshes.
    ProcessAppendList(
        ModelInstances, 
        MaterialUpdateList,
        CurrentFrameEntries, 
        CurrentFrameGeometryEntries, 
        ModelMatricesBufferAllocator, 
        TexturesIndexBufferAllocator, 
        IndirectBufferAllocator, 
        EnabledMeshCount, 
        IndirectAppendedBufferSize, 
        InsertedMeshInstanceCount
    );
    DrawMetaDataBufferAllocator.Suballocate(InsertedMeshInstanceCount * SizeOfDrawMetaData);
    VisibilityIndexBuffer.Allocator.Suballocate(InsertedMeshInstanceCount * sizeof(uint32_t));

    //Check whether the allocator allocated extra virtual memory
    //bool IsIndirectBufferReallocated = IndirectBufferCapacity < IndirectBufferAllocator.GetCapacity();
    bool IsIndirectBufferReallocated = IndirectBufferAllocator.GetCapacity() < (IndirectBufferAllocator.GetTotalFreeSpace() + IndirectAppendedBufferSize);
    bool IsModelMatrixesBufferReallocated = ModelMatricesBufferCapacity < ModelMatricesBufferAllocator.GetCapacity();
    //bool IsMeshVisibilityCountBufferReallocated = MeshVisibilityCountBufferCapacity < MeshVisibilityCountBufferAllocator.GetCapacity();
    bool IsDrawMetaDataBufferReallocated = DrawMetaDataBufferCapacity < DrawMetaDataBufferAllocator.GetCapacity();
    bool IsVisibilityBufferReallocated = VisibilityIndexBufferCapacity < VisibilityIndexBuffer.Allocator.GetCapacity();

    //Flag the other update functions 
    TexturesIndexBufferReallocated = TexturesIndexBufferCapacity < TexturesIndexBufferAllocator.GetCapacity();
    CurrentFrameEntries.TransformationMatrixReallocated = IsModelMatrixesBufferReallocated;

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
        //CopyInfos,
        IndirectBuffer,
        DrawMetaDataBuffer, 
        ModelMatricesBuffer,
        VisibilityIndexBuffer,
        SceneOptions,
        TargetDescriptorSets[FrameIndex],
        IsIndirectBufferReallocated,
        IndirectAppendedBufferSize,
        IsModelMatrixesBufferReallocated, 
        ModelMatricesBufferCapacity,
        IsDrawMetaDataBufferReallocated, 
        TexturesIndexBufferReallocated,
        IsVisibilityBufferReallocated
    );
      
    //Calculate the needed staging buffer size
    //size_t IndirectStagingBufferSize = IsIndirectBufferReallocated ? IndirectBufferAllocator.GetUsedSpace() : IndirectAppendedBufferSize;
    //size_t IndirectStagingBufferSize = IndirectBufferAllocator.GetUsedSpace() + IndirectAppendedBufferSize;
    //size_t DrawMetaDataBufferSize = IsThereDrawMetaCopyInfos ? (InsertedMeshInstanceCount * SizeOfDrawMetaData) : DrawMetaDataBufferAllocator.GetUsedSpace();
    
    /*
    size_t TotalStagingBufferSize = IndirectStagingBufferSize + DrawMetaDataBufferSize;

    //Nothing else to do. Return
    if (!TotalStagingBufferSize)
    {
        return;
    }
    */

    //Handle the allocation or reallocation of the scene persisten staging buffer
    //StagingBuffer.AllocateSceneStagingBuffer(TotalStagingBufferSize, RendererContext);

    //Create or update the indirect commands and draw meta datas
    /*CreateAppendCopyInfos(
        CopyInfos,
        CurrentFrameEntries,
        IndirectBuffer,
        DrawMetaDataBuffer,
        StagingBuffer.StagingBuffer,
        CurrentPageMeshCounts,
        GeometryBufferPageCount,
        IsIndirectBufferReallocated,
        IsThereIndirectCopyInfos, 
        IsThereDrawMetaCopyInfos
    );*/

    CreateAppendCopyInfos(
        CurrentFrameEntries,
        IndirectBuffer,
        DrawMetaDataBuffer,
        CurrentPageMeshCounts,
        GeometryBufferPageCount,
        IsIndirectBufferReallocated,
        ResourceManagerPtr
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

void ExtractMaterial(
   SCENE::INTERNAL::MaterialMetaData &MaterialMetaData,
   std::unordered_map<uint64_t, RENDERER::TextureDataEntry> &TextureDatas,
   SCENE::Material* MaterialPtr,
   uint32_t FrameIndex
)
{
    auto& MaterialData = MaterialMetaData.Material;

    MaterialData.Parameters.Albedo = MaterialPtr->Albedo;
    MaterialData.Parameters.Metallic = MaterialPtr->Metallic;
    MaterialData.Parameters.Roughness = MaterialPtr->Roughness;

    MaterialData.SamplingData.TextureSampleSize = MaterialPtr->TextureSampleSize;
    MaterialData.SamplingData.TextureSamplePosition = MaterialPtr->TextureSamplePosition;
    for (uint32_t i = 0; i < static_cast<uint32_t>(SCENE::MATERIAL_TEXTURE_TYPE_META_DATA_SIZE); i++)
    {
        auto TextureIndex = MaterialPtr->GetTexture(static_cast<SCENE::MaterialTextureType>(i));
        if (TextureIndex)
        {
            auto Iterator = TextureDatas.find(TextureIndex);
            if (Iterator == TextureDatas.end())
            {
                MaterialData.IndexData.TextureIndexes[i] = -1;
                continue;
            }
            auto& TextureDataEntry = Iterator->second;
            MaterialData.IndexData.TextureIndexes[i] = TextureDataEntry.DescriptorSlots[FrameIndex];
        }
        else MaterialData.IndexData.TextureIndexes[i] = -1;
    }
}

//Updating texture index buffer which links materials with the actual texture descriptor slots 
void SCENE::SceneMeshManager::UpdateMaterials(
    std::vector<SCENE::ModelInstance*> &SceneMaterialUpdateList,
    uint32_t FrameIndex,
    VkDescriptorSet& TargetDescriptorSet
    //PersistentStagingBuffer& StagingBuffer
    //std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)>& CopyOperations
)
{
    //this->ResourceManagerPtr->TextureManager.UpdateDescriptors(FrameIndex);
    auto& CurrentFrameEntries = Entries[FrameIndex];
    if (SceneMaterialUpdateList.empty()) return;

    auto& CurrentFrameGeometryEntries = this->ResourceManagerPtr->MeshManager.GeometryEntries[FrameIndex];

    auto& CurrentTextureIndexBuffer = Buffers.TexturesIndexBuffers[FrameIndex];
    auto& TextureIndexBufferAllocator = CurrentTextureIndexBuffer.Allocator;
    auto Capacity = TextureIndexBufferAllocator.GetCapacity() - TextureIndexBufferAllocator.GetTotalFreeSpace();
    auto& TexturesIndexBufferReallocated = Buffers.TexturesIndexBuffersReallocated[FrameIndex];

    //bool IsThereTextureIndexCopyInfos = !CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.empty();

    uint32_t MaterialTextureTypeCount = static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE);
    size_t SizeOfMaterialData = sizeof(INTERNAL::MaterialData);
    
    //Material meta data caches to avoid fetching the datas from the unordered map again.
    std::vector<SCENE::INTERNAL::MaterialMetaData*> InstanceMaterialMetaDataCaches;
    InstanceMaterialMetaDataCaches.reserve(SceneMaterialUpdateList.size() * 10);
    //Handle material update lists and extract materials
    RENDERER_CORE::BufferCopyInfo CopyInfo{};
    for (auto& ModelInstancePtr : SceneMaterialUpdateList)
    {
        //Fetch the instance entries belonging to this instance ID
        auto InstanceEntryIterator = CurrentFrameEntries.InstanceEntries.find(ModelInstancePtr->GetHandleID());
        if (!InstanceEntryIterator) continue;

        for (size_t i = 0; i < ModelInstancePtr->Materials.size(); i++)
        {
            auto& InstanceMaterial = ModelInstancePtr->Materials[i];
            //Pick the right material
            /*
            size_t MeshGeometryID = ModelInstancePtr->Source->Meshes[i].GeometryID;
            if (ModelInstancePtr->Materials.size() > i)
            {
                MaterialPtr = &ModelInstancePtr->Materials[i];
            }
            else
            {
                auto& GeometryEntryIterator = CurrentFrameGeometryEntries.find(MeshGeometryID);
                if (GeometryEntryIterator == CurrentFrameGeometryEntries.end())
                {
                    throw std::runtime_error("Attempt on linking non-existent mesh instance!");
                }
                MaterialPtr = &GeometryEntryIterator->second.MeshMaterial;
            }
            */

            auto& MaterialMetaData = InstanceEntryIterator->second.Materials[ModelInstancePtr->Source->Meshes[i].GeometryID];
            //Extract the material data
            ExtractMaterial(
                MaterialMetaData,
                this->ResourceManagerPtr->TextureManager.TextureDatas,
                &InstanceMaterial,
                FrameIndex
            );

            //Push back the material cache
            //Since all of the traversed containers are vectors, caches will be in the pushed order. 
            InstanceMaterialMetaDataCaches.push_back(&MaterialMetaData);
        }
    }
    
    //In case the material buffer reallocated provide buffer growth
    if (TexturesIndexBufferReallocated)
    {
        RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice,CurrentTextureIndexBuffer.Buffer);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.PhysicalDevice,
            RendererContext->DeviceContext.LogicalDevice,
            TextureIndexBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CurrentTextureIndexBuffer.Buffer
        );

        RENDERER_CORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
            CurrentTextureIndexBuffer.Buffer,
            TextureIndexBufferAllocator.GetCapacity(),
            0,
            TargetDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );

        RENDERER_CORE::WriteDescriptorSets(
            RendererContext->DeviceContext.LogicalDevice,
            { DrawMetaDataBufferWrite },
            {}
        );
       // CopyOperations[TEXTUREINDEX_COPY]->DestinationBuffer = &CurrentTextureIndexBuffer.Buffer;
    }

   // RENDERER_CORE::PersistentBuffer TextureIndexStagingBuffer;
    /*
    size_t StagingBufferSize = TexturesIndexBufferReallocated ? Capacity :
                                SceneMaterialUpdateList.size() * SizeOfMaterialData;

    StagingBuffer.AllocateSceneStagingBuffer(StagingBufferSize, RendererContext);
    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.StagingBuffer.Buffer.MappedMemory);
    if (!StagingBufferPtr) return;
    */
    std::vector<SCENE::INTERNAL::MaterialData> AppendedMaterialDatas;
    std::vector<VkBufferCopy> AppendedMaterialDataCopyRegions;

    //Create the actual copy infos
    //In case the buffer was reallocated, all existing data must be copied inside again.
    if (TexturesIndexBufferReallocated)
    {
        //if(IsThereTextureIndexCopyInfos) CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.clear();
        //CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.reserve(CurrentFrameEntries.InstanceEntries.size() * 10);
        for (auto& [Handle,InstanceEntry] : CurrentFrameEntries.InstanceEntries)
        {
            for (auto& [MeshGeometryHandle, MetaData] : InstanceEntry.Materials)
            {
                /*
                //Reallocate a memory region from the staging buffer if staging buffer was reset or it's first time
                if (!IsThereTextureIndexCopyInfos || !MetaData.StagingTextureIndexMemoryRegion.Size)
                {
                    MetaData.StagingTextureIndexMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfMaterialData);
                }
                memcpy(StagingBufferPtr + MetaData.StagingTextureIndexMemoryRegion.Offset, &MetaData.Material, MetaData.StagingTextureIndexMemoryRegion.Size);
                */
                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MetaData.TextureIndexMemoryRegion.Offset;
                CopyRegion.size = SizeOfMaterialData;
                CopyRegion.srcOffset = AppendedMaterialDatas.size() * SizeOfMaterialData;
                //CopyRegion.srcOffset = MetaData.StagingTextureIndexMemoryRegion.Offset;
                //CopyRegion.size = MetaData.StagingTextureIndexMemoryRegion.Size;

                AppendedMaterialDataCopyRegions.push_back(std::move(CopyRegion));
                AppendedMaterialDatas.push_back(MetaData.Material);
                //CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.push_back(std::move(CopyRegion));
            }
        }
    }
    else
    {
        //Copy only the related ones.
       // CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.reserve(SceneMaterialUpdateList.size() * 10);
        size_t MaterialCacheIterator = 0;
        for (auto& ModelInstancePtr : SceneMaterialUpdateList)
        {
            for (size_t y = 0; y < ModelInstancePtr->Materials.size(); y++)
            {
                //Fetch the cached data
                auto& MaterialCache = InstanceMaterialMetaDataCaches[MaterialCacheIterator];

                /*
                //Reallocate a memory region from the staging buffer if staging buffer was reset or it's first time
                if (!IsThereTextureIndexCopyInfos || !MaterialCache->StagingTextureIndexMemoryRegion.Size)
                {
                    MaterialCache->StagingTextureIndexMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfMaterialData);
                }
                memcpy(StagingBufferPtr + MaterialCache->StagingTextureIndexMemoryRegion.Offset, &MaterialCache->Material, MaterialCache->StagingTextureIndexMemoryRegion.Size);
                */
                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MaterialCache->TextureIndexMemoryRegion.Offset;
                CopyRegion.size = SizeOfMaterialData;
                CopyRegion.srcOffset = AppendedMaterialDatas.size() * SizeOfMaterialData;
                //CopyRegion.srcOffset = MaterialCache->StagingTextureIndexMemoryRegion.Offset;
                //CopyRegion.size = MaterialCache->StagingTextureIndexMemoryRegion.Size;
                //CopyOperations[TEXTUREINDEX_COPY]->CopyRegions.push_back(std::move(CopyRegion));
                AppendedMaterialDataCopyRegions.push_back(std::move(CopyRegion));
                AppendedMaterialDatas.push_back(MaterialCache->Material);

                MaterialCacheIterator++;
            }
        }
    }

    if (!AppendedMaterialDatas.empty())
    {
        std::shared_ptr<RENDERER::DataBlock> MaterialDataBlock = std::make_shared<RENDERER::DataBlock>();
        MaterialDataBlock->DataPtr = reinterpret_cast<uint8_t*>(AppendedMaterialDatas.data());
        MaterialDataBlock->SizeInBytes = AppendedMaterialDatas.size() * SizeOfMaterialData;
        MaterialDataBlock->Deleter = [LocalVector = std::move(AppendedMaterialDatas)]() {};

        ResourceManagerPtr->RequestCopyOperation(
            AppendedMaterialDataCopyRegions,
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &CurrentTextureIndexBuffer.Buffer,
            MaterialDataBlock,
            2,
            RENDERER::COPY_OPERATION_FLAG_ATOMIC
        );
    }

    //Clean slate.
    TexturesIndexBufferReallocated = false;
}
