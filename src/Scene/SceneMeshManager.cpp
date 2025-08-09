#include "SceneMeshManager.hpp"

#include "Mesh.hpp"
#include "ModelInstance.hpp"
#include "../Renderer/RendererContext.hpp"
#include "MaterialManager.hpp"
#include "Scene.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

enum BufferCopySlots {
    VERTEX_COPY = 0,
    INDEX_COPY = 1,
    INDIRECT_COPY = 2,
    DRAWMETA_COPY = 3
};

SCENE::MeshManager::MeshManager(RENDERER::RendererContext &RendererContext)
{
    Create(RendererContext);
}

void SCENE::MeshManager::Create(RENDERER::RendererContext& RendererContext)
{
    this->RendererContext = &RendererContext;
    SceneMeshIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    PerformanceModeBuffers.Create();
}

void SCENE::MeshManager::Destroy(VkDevice& LogicalDevice)
{
    PerformanceModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //BalancedModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //MemorySavingModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        SceneMeshIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
    }
}

void SCENE::MeshManager::UpdateMeshTransformations(std::vector<ModelInstance*>& UpdateList, uint32_t CurrentFrame)
{
    if (UpdateList.empty()) return;
    uint8_t* Destination = reinterpret_cast<uint8_t*>(PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame].Buffer.MappedMemory);
    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame].ModelEntries;

    for (auto& ModelInstance : UpdateList)
    {
        auto& AllocatedMemoryRegion = CurrentFrameModelEntries[ModelInstance->Source].Instances[ModelInstance];
        glm::mat4 ModelMatrix = ModelInstance->Transformations.GetModelMatrix();
        memcpy(Destination + AllocatedMemoryRegion[0].Offset,
            &ModelMatrix,
            AllocatedMemoryRegion[0].Size
        );
    }

   /*
    for (auto &[ModelPtr,ModelEntry] : CurrentFrameModelEntries)
    {
        for (auto& [InstancePtr, AllocatedMemoryRegion] : ModelEntry.Instances)
        {
            glm::mat4 ModelMatrix = InstancePtr->Transformations.GetModelMatrix();
            memcpy(Destination + AllocatedMemoryRegion[0].Offset,
                &ModelMatrix,
                AllocatedMemoryRegion[0].Size
            );
        }
    }
    */
}

void SCENE::MeshManager::AppendModels(MeshAppendInfo Info)
{
    auto& CurrentFrame = Info.FrameIndex;
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

    size_t VertexSize, IndexSize,IndirectSize, VertexBufferSize = 0,IndexBufferSize = 0,IndirectBufferSize = 0;
    size_t SizeOfVertex = sizeof(Vertex3D), 
        SizeOfUint32 = sizeof(uint32_t),
        SizeOfMat4 = sizeof(glm::mat4), 
        SizeOfDrawMetaData = sizeof(DrawMetadata), 
        SizeOfIndirectCommand = sizeof(ExtendedIndirectCommand);

    size_t VertexBufferCapacity = VertexBufferAllocator.GetCapacity(),
            IndexBufferCapacity = IndexBufferAllocator.GetCapacity(),
            IndirectBufferCapacity = IndirectBufferAllocator.GetCapacity(),
            ModelMatricesBufferCapacity = ModelMatricesBufferAllocator.GetCapacity(),
            DrawMetaDataBufferCapacity = DrawMetaDataBufferAllocator.GetCapacity();

    size_t InsertedInstanceCount = 0 , InsertedMeshInstanceCount = 0;

    //auto& CurrentFrameMeshEntries = MeshEntries[CurrentFrame];
    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame];
    //Process newly inserted meshes.
    std::set<Mesh*> InsertedMetaDatas;
    for (auto& ModelInstance : Info.ModelInstances)
    {
        if (!ModelInstance || !ModelInstance->Source) continue;

        //Handle multiple references to the model
        auto& ModelIterator = CurrentFrameModelEntries.ModelEntries.find(ModelInstance->Source);
        bool DoesModelAlreadyExist = ModelIterator != CurrentFrameModelEntries.ModelEntries.end();

        std::array<RENDERER_CORE::MemoryRegion, 2> NewInstanceMemoryRegions;
        NewInstanceMemoryRegions[0] = ModelMatricesBufferAllocator.Allocate(SizeOfMat4);

        InsertedInstanceCount++;
        if (DoesModelAlreadyExist)
        {
            InsertedMeshInstanceCount += ModelIterator->second.MeshEntries.size();
            ModelIterator->second.MetaData.ReferenceCount++;
            ModelIterator->second.MetaData.IsChanged = true;
            ModelIterator->second.Instances[ModelInstance] = NewInstanceMemoryRegions;
            continue;
        }

        //Register the instance and its model
        ModelMetaData NewModelData;
        NewModelData.ReferenceCount++;

        auto &NewModelEntry = CurrentFrameModelEntries.ModelEntries[ModelInstance->Source];
        NewModelEntry.MetaData = NewModelData;
        NewModelEntry.Instances.emplace(ModelInstance, NewInstanceMemoryRegions);

        //Process and allocate the individual meshes 
        for (auto& Mesh : ModelInstance->Source->Meshes)
        {
            VertexSize = Mesh.Vertices.size() * SizeOfVertex;
            IndexSize = Mesh.Indices.size() * SizeOfUint32;

            MeshMetaData MetaData{};
            MetaData.MemoryRegions[0] = VertexBufferAllocator.Allocate(VertexSize);
            MetaData.MemoryRegions[1] = IndexBufferAllocator.Allocate(IndexSize);
            MetaData.MemoryRegions[2] = IndirectBufferAllocator.Allocate(SizeOfIndirectCommand);

            MetaData.DrawInfo.VertexOffset = MetaData.MemoryRegions[0].Offset / SizeOfVertex;
            MetaData.DrawInfo.FirstIndex = MetaData.MemoryRegions[1].Offset / SizeOfUint32;
            MetaData.DrawInfo.IndexCount = Mesh.Indices.size();
            MetaData.ModelInstance = ModelInstance;

            NewModelEntry.MeshEntries.push_back({ &Mesh , MetaData });
            InsertedMetaDatas.insert(&Mesh);

            VertexBufferSize += VertexSize;
            IndexBufferSize += IndexSize;
            IndirectBufferSize += SizeOfIndirectCommand;

            EnabledMeshCount++;
        }
        InsertedMeshInstanceCount += NewModelEntry.MeshEntries.size();
    }
    //ModelMatricesBufferAllocator.Allocate(SizeOfMat4 * InsertedInstanceCount);
    DrawMetaDataBufferAllocator.Allocate(InsertedMeshInstanceCount * SizeOfDrawMetaData);

    //Check whether the allocator allocated extra virtual memory
    bool IsVertexBufferReallocated = VertexBufferCapacity < VertexBufferAllocator.GetCapacity(),
        IsIndexBufferReallocated = IndexBufferCapacity < IndexBufferAllocator.GetCapacity(),
        IsIndirectBufferReallocated = IndirectBufferCapacity < IndirectBufferAllocator.GetCapacity(),
        IsModelMatrixesBufferReallocated = ModelMatricesBufferCapacity < ModelMatricesBufferAllocator.GetCapacity(),
        IsDrawMetaDataBufferReallocated = DrawMetaDataBufferCapacity < DrawMetaDataBufferAllocator.GetCapacity();

    std::vector<RENDERER_CORE::BufferCopyInfo> CopyInfos(4);
    std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> WriteInfos(3);
    //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
    if (IsVertexBufferReallocated)
    {
        VertexBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            VertexBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VertexBuffer.Buffer
        );
    }
    if (IsIndexBufferReallocated)
    {
        IndexBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            IndexBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            IndexBuffer.Buffer
        );
    }
    if (IsIndirectBufferReallocated)
    {
        IndirectBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            IndirectBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            IndirectBuffer.Buffer
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
        ModelMatricesBuffer.Buffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            ModelMatricesBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
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
        DrawMetaDataBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            DrawMetaDataBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            DrawMetaDataBuffer.Buffer
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
        
    size_t VertexStagingBufferSize = IsVertexBufferReallocated ? VertexBufferAllocator.GetCapacity() : VertexBufferSize,
        IndexStagingBufferSize = IsIndexBufferReallocated ? IndexBufferAllocator.GetCapacity() : IndexBufferSize,
        IndirectStagingBufferSize = IsIndirectBufferReallocated ? IndirectBufferAllocator.GetCapacity() : IndirectBufferSize,
        DrawMetaDataBufferSize = DrawMetaDataBufferAllocator.GetCapacity();
        
    size_t TotalStagingBufferSize = VertexStagingBufferSize + IndexStagingBufferSize + IndirectStagingBufferSize + DrawMetaDataBufferSize;
    if (!TotalStagingBufferSize)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed model appending operation, faulty staging buffer!");
        throw std::runtime_error("Faulty staging buffer!");
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

    //Global draw index iterator
    size_t DrawIndexIterator = 0;
    //Global mesh index iterator
    uint32_t CurrentMesh = 0;
   // ModelMatricesBufferAllocator.Reset();
    DrawMetaDataBufferAllocator.Reset();

    //Local model index iterator
    size_t ModelIterator = 0;
    for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
    {
        //Produce necessary copy informations
        for (auto& MeshEntry : ModelEntryMetaData.MeshEntries)
        {
            auto& MeshEntryPtr = MeshEntry.MeshPtr;
            auto& MeshEntryMetaData = MeshEntry.MetaData;
            bool IsMeshJustInserted = InsertedMetaDatas.find(MeshEntryPtr) != InsertedMetaDatas.end();
            
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

            if (IsVertexBufferReallocated || IsMeshJustInserted)
            {
                VertexSize = MeshEntryPtr->Vertices.size() * SizeOfVertex;
                RENDERER_CORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(VertexSize);

                memcpy(StagingBufferPtr + AllocatedRegion.Offset, MeshEntryPtr->Vertices.data(), AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MeshEntryMetaData.MemoryRegions[0].Offset;
                CopyRegion.size = AllocatedRegion.Size;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[VERTEX_COPY].CopyRegions.push_back(CopyRegion);
            }
            if (IsIndexBufferReallocated || IsMeshJustInserted)
            {
                IndexSize = MeshEntryPtr->Indices.size() * SizeOfUint32;
                RENDERER_CORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(IndexSize);
                memcpy(StagingBufferPtr + AllocatedRegion.Offset, MeshEntryPtr->Indices.data(), AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MeshEntryMetaData.MemoryRegions[1].Offset;
                CopyRegion.size = IndexSize;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[INDEX_COPY].CopyRegions.push_back(CopyRegion);
            }
            if (ModelEntryMetaData.MetaData.IsChanged || IsIndirectBufferReallocated || IsMeshJustInserted)
            {
                ModelEntryMetaData.MetaData.IsChanged = false;

                ExtendedIndirectCommand NewIndirectCommand{};
                NewIndirectCommand.IndexCount = MeshEntryMetaData.DrawInfo.IndexCount;
                NewIndirectCommand.FirstIndex = MeshEntryMetaData.DrawInfo.FirstIndex;
                NewIndirectCommand.InstanceCount = ModelEntryMetaData.MetaData.ReferenceCount;
                NewIndirectCommand.VertexOffset = MeshEntryMetaData.DrawInfo.VertexOffset;
                NewIndirectCommand.FirstInstance = DrawIndexIterator;
                NewIndirectCommand.MeshIndex = ModelEntryMetaData.MetaData.Index;

                RENDERER_CORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(SizeOfIndirectCommand);
                memcpy(StagingBufferPtr + AllocatedRegion.Offset, &NewIndirectCommand, AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MeshEntryMetaData.MemoryRegions[2].Offset;
                CopyRegion.size = AllocatedRegion.Size;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[INDIRECT_COPY].CopyRegions.push_back(CopyRegion);
            }
            DrawIndexIterator += ModelEntryMetaData.MetaData.ReferenceCount;
            CurrentMesh++;
        }
        ModelIterator += ModelEntryMetaData.MetaData.ReferenceCount;
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
}

void SCENE::MeshManager::EraseModels(MeshEraseInfo Info)
{
    auto& CurrentFrame = Info.FrameIndex;

    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame];
    if (!CurrentFrameModelEntries.ModelEntries.size())
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_WARNING, "Invalid model erasing operation. No existing model detected. Returning...");
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
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed model erasing operation, faulty staging buffer!");
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
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_ERROR, "Unable to proceed model erasing operation, failed staging buffer creation!");
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
                    NewIndirectCommand.MeshIndex = ModelEntryMetaData.MetaData.Index;
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
}

void SCENE::MeshManager::WriteTexture(
    uint32_t TextureTypeIndex,
    SCENE::Mesh& Mesh,
    SCENE::TextureImportManager& TextureImportManager,
    std::vector<RENDERER_CORE::DescriptorSetWriteImage>& ImageWrites,
    std::vector<int>& TextureIndexes,
    int& CurrentImageIndex,
    VkDescriptorSet DestinationDescriptorSet
)
{
    auto TextureIndex = Mesh.MeshMaterial.GetTexture(static_cast<MaterialTextureType>(TextureTypeIndex));
    if (TextureIndex)
    {
        auto Iterator = TextureImportManager.TextureDatas.find(TextureIndex);
        if (Iterator == TextureImportManager.TextureDatas.end())
        {
            TextureIndexes.push_back(-1);
            return;
        }
        auto& TextureData = TextureImportManager.TextureDatas[TextureIndex];

        RENDERER_CORE::DescriptorSetWriteImage NewTextureWrite(
            TextureData.ImageView,
            TextureData.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            DestinationDescriptorSet,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            CurrentImageIndex,
            1
        );
        ImageWrites.push_back(std::move(NewTextureWrite));
        TextureIndexes.push_back(CurrentImageIndex);
        CurrentImageIndex++;
    }
    else TextureIndexes.push_back(-1);
}

void SCENE::MeshManager::UpdateTextureDescriptors(MeshTextureUpdateInfo Info)
{
    auto FrameIndex = Info.FrameIndex;
    auto& TextureImportManager = *Info.TextureImportManagerPtr;
    auto& DescriptorSets = Info.TargetDescriptorSets;

    auto& CurrentDescriptorSet = DescriptorSets[FrameIndex];

    std::vector<RENDERER_CORE::DescriptorSetWriteImage> ImageWrites;
    std::vector<int> TextureIndexes;
    int CurrentImageIndex = 0;
    for (auto& [ModelPtr, ModelEntry] : ModelEntries[FrameIndex].ModelEntries)
    {
        for (auto& Mesh : ModelPtr->Meshes)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE); i++)
            {
                WriteTexture(
                    i,
                    Mesh,
                    TextureImportManager,
                    ImageWrites,
                    TextureIndexes,
                    CurrentImageIndex,
                    CurrentDescriptorSet
                );
            }
        }
    }

    if (!CurrentImageIndex)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_WARNING, "Invalid texture descriptor writing operation. No existing texture detected. Returning...");
        return;
    }

    auto& CurrentTextureIndexBuffer = PerformanceModeBuffers.TexturesIndexBuffers[FrameIndex];
    auto& TextureIndexBufferAllocator = CurrentTextureIndexBuffer.Allocator;
    auto OldCapacity = TextureIndexBufferAllocator.GetCapacity();
    TextureIndexBufferAllocator.Reset();
    VkDeviceSize BufferSize = sizeof(int) * TextureIndexes.size();
    TextureIndexBufferAllocator.Allocate(BufferSize);

    std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> BufferWrites;
    if (TextureIndexBufferAllocator.GetCapacity() > OldCapacity)
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
            1,
            CurrentDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        BufferWrites.push_back(DrawMetaDataBufferWrite);
    }

    RENDERER_CORE::WriteDescriptorSets(
        RendererContext->DeviceContext.logicalDevice,
        BufferWrites,
        ImageWrites
    );

    RENDERER_CORE::PersistentBuffer TextureIndexStagingBuffer;
    RENDERER_CORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        TextureIndexBufferAllocator.GetCapacity(),
        TextureIndexStagingBuffer.Buffer
    );
    TextureIndexStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0,TextureIndexBufferAllocator.GetCapacity(), 0);

    memcpy(TextureIndexStagingBuffer.MappedMemory, TextureIndexes.data(), BufferSize);
  
    RENDERER_CORE::CopyBuffer(
        TextureIndexStagingBuffer.Buffer.BufferObject,
        CurrentTextureIndexBuffer.Buffer.BufferObject,
        BufferSize,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    TextureIndexStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}
