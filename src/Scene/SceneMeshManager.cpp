#include "SceneMeshManager.hpp"

#include "Mesh.hpp"
#include "ModelInstance.hpp"
#include "../Renderer/RendererContext.hpp"
#include "MaterialManager.hpp"
#include "Scene.hpp"

enum BufferCopySlots {
    VERTEX_COPY = 0,
    INDEX_COPY = 1,
    INDIRECT_COPY = 2,
    DRAWMETA_COPY = 3
};


VKSCENE::MeshManager::MeshManager(VKAPP::RendererContext &RendererContext)
{
    Create(RendererContext);
}

void VKSCENE::MeshManager::Create(VKAPP::RendererContext& RendererContext)
{
    this->RendererContext = &RendererContext;
    SceneMeshIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    PerformanceModeBuffers.Create();
}

void VKSCENE::MeshManager::Destroy(VkDevice& LogicalDevice)
{
    PerformanceModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //BalancedModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    //MemorySavingModeBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        SceneMeshIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
    }
}

void VKSCENE::MeshManager::UpdateMeshTransformations(uint32_t CurrentFrame)
{
    uint8_t* Destination = reinterpret_cast<uint8_t*>(PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame].Buffer.MappedMemory);
    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame].ModelEntries;
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
}

void VKSCENE::MeshManager::AppendModels(MeshAppendInfo Info)
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

        InsertedInstanceCount++;
        if (DoesModelAlreadyExist)
        {
            InsertedMeshInstanceCount += ModelIterator->second.MeshEntries.size();
            ModelIterator->second.MetaData.ReferenceCount++;
            ModelIterator->second.Instances.emplace(ModelInstance, std::array<VKCORE::MemoryRegion, 2>());
            continue;
        }

        //Register the instance and its model
        ModelMetaData NewModelData;
        NewModelData.ReferenceCount++;

        auto &NewModelEntry = CurrentFrameModelEntries.ModelEntries[ModelInstance->Source];
        NewModelEntry.MetaData = NewModelData;
        NewModelEntry.Instances.emplace(ModelInstance, std::array<VKCORE::MemoryRegion, 2>());

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
    ModelMatricesBufferAllocator.Allocate(SizeOfMat4 * InsertedInstanceCount);
    DrawMetaDataBufferAllocator.Allocate(InsertedMeshInstanceCount * SizeOfDrawMetaData);

    //Check whether the allocator allocated extra virtual memory
    bool IsVertexBufferReallocated = VertexBufferCapacity < VertexBufferAllocator.GetCapacity(),
        IsIndexBufferReallocated = IndexBufferCapacity < IndexBufferAllocator.GetCapacity(),
        IsIndirectBufferReallocated = IndirectBufferCapacity < IndirectBufferAllocator.GetCapacity(),
        IsModelMatrixesBufferReallocated = ModelMatricesBufferCapacity < ModelMatricesBufferAllocator.GetCapacity(),
        IsDrawMetaDataBufferReallocated = DrawMetaDataBufferCapacity < DrawMetaDataBufferAllocator.GetCapacity();

    std::vector<VKCORE::BufferCopyInfo> CopyInfos(4);
    std::vector<VKCORE::DescriptorSetWriteBuffer> WriteInfos(3);
    //In case the buffer needs be enlarged , create a new buffer and copy existing data into it.
    if (IsVertexBufferReallocated)
    {
        VertexBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        VKCORE::CreateBuffer(
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
        VKCORE::CreateBuffer(
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
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            IndirectBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            IndirectBuffer.Buffer
        );

        VKCORE::DescriptorSetWriteBuffer IndirectBufferWrite(
            IndirectBuffer.Buffer,
            IndirectBufferAllocator.GetCapacity(),
            1,
            Info.TargetDescriptorSets[CurrentFrame],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { IndirectBufferWrite }, {});
    }
    if (IsModelMatrixesBufferReallocated)
    {
        ModelMatricesBuffer.Buffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            ModelMatricesBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            ModelMatricesBuffer.Buffer.Buffer
        );
        ModelMatricesBuffer.Buffer.Map(RendererContext->DeviceContext.logicalDevice, 0, ModelMatricesBufferAllocator.GetCapacity(), 0);

        VKCORE::DescriptorSetWriteBuffer ModelMatrixBufferWrite(
            ModelMatricesBuffer.Buffer.Buffer,
            ModelMatricesBufferAllocator.GetCapacity(), 
            0, 
            Info.TargetDescriptorSets[CurrentFrame], 
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { ModelMatrixBufferWrite }, {});
    }
    if (IsDrawMetaDataBufferReallocated)
    {
        DrawMetaDataBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            DrawMetaDataBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            DrawMetaDataBuffer.Buffer
        );

        VKCORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
            DrawMetaDataBuffer.Buffer,
            DrawMetaDataBufferAllocator.GetCapacity(),
            2,
            Info.TargetDescriptorSets[CurrentFrame],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { DrawMetaDataBufferWrite }, {});
    }
        
    size_t VertexStagingBufferSize = IsVertexBufferReallocated ? VertexBufferAllocator.GetCapacity() : VertexBufferSize,
        IndexStagingBufferSize = IsIndexBufferReallocated ? IndexBufferAllocator.GetCapacity() : IndexBufferSize,
        IndirectStagingBufferSize = IsIndirectBufferReallocated ? IndirectBufferAllocator.GetCapacity() : IndirectBufferSize,
        DrawMetaDataBufferSize = DrawMetaDataBufferAllocator.GetCapacity();
        
    size_t TotalStagingBufferSize = VertexStagingBufferSize + IndexStagingBufferSize + IndirectStagingBufferSize + DrawMetaDataBufferSize;

    VKCORE::PersistentBuffer StagingBuffer{};
    VKCORE::VirtualArenaAllocator StagingBufferAllocator(TotalStagingBufferSize);
    VKCORE::CreateStagingBuffer(
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
    ModelMatricesBufferAllocator.Reset();
    DrawMetaDataBufferAllocator.Reset();

    //Local model index iterator
    size_t ModelIterator = 0;
    for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
    {
        //Redistribute model matrixes
        for (auto& [InstancePtr,AllocatedMemoryRegion] : ModelEntryMetaData.Instances)
        {
            AllocatedMemoryRegion[0] = ModelMatricesBufferAllocator.Allocate(SizeOfMat4);
        }

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
                NewDrawMetaData.ModelMatrixIndex = ModelIterator + InstanceIterator;
                
                VKCORE::MemoryRegion DrawMetaDataAllocatedRegion = DrawMetaDataBufferAllocator.Allocate(SizeOfDrawMetaData);
                VKCORE::MemoryRegion DrawMetaDataStagingAllocatedRegion = StagingBufferAllocator.Allocate(SizeOfDrawMetaData);
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
                VKCORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(VertexSize);

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
                VKCORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(IndexSize);
                memcpy(StagingBufferPtr + AllocatedRegion.Offset, MeshEntryPtr->Indices.data(), AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = MeshEntryMetaData.MemoryRegions[1].Offset;
                CopyRegion.size = IndexSize;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[INDEX_COPY].CopyRegions.push_back(CopyRegion);
            }
            if (IsIndirectBufferReallocated || IsMeshJustInserted)
            {
                ExtendedIndirectCommand NewIndirectCommand{};
                NewIndirectCommand.IndexCount = MeshEntryMetaData.DrawInfo.IndexCount;
                NewIndirectCommand.FirstIndex = MeshEntryMetaData.DrawInfo.FirstIndex;
                NewIndirectCommand.InstanceCount = ModelEntryMetaData.MetaData.ReferenceCount;
                NewIndirectCommand.VertexOffset = MeshEntryMetaData.DrawInfo.VertexOffset;
                NewIndirectCommand.FirstInstance = DrawIndexIterator;
                NewIndirectCommand.MeshIndex = ModelEntryMetaData.MetaData.Index;

                VKCORE::MemoryRegion AllocatedRegion = StagingBufferAllocator.Allocate(SizeOfIndirectCommand);
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
    VKCORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.logicalDevice, 
        RendererContext->CommandPool.commandPool, 
        RendererContext->DeviceContext.GraphicsQueue
    );

    StagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::MeshManager::EraseModels(MeshEraseInfo Info)
{
    auto& CurrentFrame = Info.FrameIndex;
    auto& EnabledMeshCount = PerformanceModeBuffers.EnabledMeshCount[CurrentFrame];

    auto& VertexBuffer = PerformanceModeBuffers.VertexBuffers[CurrentFrame];
    auto& IndexBuffer = PerformanceModeBuffers.IndexBuffers[CurrentFrame];
    auto& IndirectBuffer = PerformanceModeBuffers.IndirectBuffers[CurrentFrame];
    auto& ModelMatricesBuffer = PerformanceModeBuffers.ModelMatricesBuffers[CurrentFrame];

    auto& VertexBufferAllocator = VertexBuffer.Allocator;
    auto& IndexBufferAllocator = IndexBuffer.Allocator;
    auto& IndirectBufferAllocator = IndirectBuffer.Allocator;
    auto& ModelMatricesBufferAllocator = ModelMatricesBuffer.Allocator;

    //auto& CurrentFrameMeshEntries = MeshEntries[CurrentFrame];
    auto& CurrentFrameModelEntries = ModelEntries[CurrentFrame];

    for (auto& ModelInstancePtr : Info.ModelInstances)
    {
        if (!ModelInstancePtr || !ModelInstancePtr->Source) continue;

        auto& ModelIterator = CurrentFrameModelEntries.ModelEntries.find(ModelInstancePtr->Source);
        if (ModelIterator == CurrentFrameModelEntries.ModelEntries.end()) continue;

        ModelIterator->second.MetaData.ReferenceCount--;
        ModelIterator->second.Instances.erase(ModelInstancePtr);

        if (ModelIterator->second.MetaData.ReferenceCount) continue;

        for (auto& MeshEntry : ModelIterator->second.MeshEntries)
        {
            const VKCORE::MemoryRegion& VertexMemoryRegion = MeshEntry.MetaData.MemoryRegions[0];
            const VKCORE::MemoryRegion& IndexMemoryRegion = MeshEntry.MetaData.MemoryRegions[1];
            const VKCORE::MemoryRegion& IndirectMemoryRegion = MeshEntry.MetaData.MemoryRegions[2];

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

    std::vector<VKCORE::MemoryRegion*> VertexAllocatedRegions;
    std::vector<VKCORE::MemoryRegion*> IndexAllocatedRegions;
    std::vector<VKCORE::MemoryRegion*> IndirectCommandAllocatedRegions;
    VertexAllocatedRegions.reserve(EnabledMeshCount);
    IndexAllocatedRegions.reserve(EnabledMeshCount);
    IndirectCommandAllocatedRegions.reserve(EnabledMeshCount);

    bool SkipDefragmentation = !VertexBufferNeedsDefragmentation && !IndexBufferNeedsDefragmentation && !IndirectBufferNeedsDefragmentation;

    //Redistribute the model matrixes.
    size_t MatrixIndexIterator = 0;
    ModelMatricesBufferAllocator.Reset();
    for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
    {
        ModelEntryMetaData.MetaData.Index = MatrixIndexIterator;
        for (auto& [InstancePtr, AllocatedMemoryRegion] : ModelEntryMetaData.Instances)
        {
            AllocatedMemoryRegion[0] = ModelMatricesBufferAllocator.Allocate(sizeof(glm::mat4));
        }

        MatrixIndexIterator += ModelEntryMetaData.MetaData.ReferenceCount;

        if (SkipDefragmentation) continue;

        //Gather memory regions to defragment. 
        for (auto& MeshEntry : ModelEntryMetaData.MeshEntries)
        {
            VertexAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[0]);
            IndexAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[1]);
            IndirectCommandAllocatedRegions.push_back(&MeshEntry.MetaData.MemoryRegions[2]);
        }
    }

 
    //No further processing is needed for now
    if (SkipDefragmentation) return;

    VKCORE::PersistentBuffer VertexStagingBuffer;
    VKCORE::PersistentBuffer IndexStagingBuffer;
    VKCORE::PersistentBuffer IndirectStagingBuffer;

    size_t VertexStagingBufferSize = VertexBufferAllocator.GetCapacity() - VertexBufferAllocator.GetTotalFreeSpace(),
            IndexStagingBufferSize = IndexBufferAllocator.GetCapacity() - IndexBufferAllocator.GetTotalFreeSpace(),
            IndirectStagingBufferSize = IndirectBufferAllocator.GetCapacity() - IndirectBufferAllocator.GetTotalFreeSpace();

    uint8_t* VertexStagingBufferPtr = nullptr, *IndexStagingBufferPtr = nullptr, *IndirectStagingBufferPtr = nullptr;

    if (VertexBufferNeedsDefragmentation)
    {
        //Create a staging buffer big enough to cover all the vertices 
        VKCORE::CreateStagingBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            VertexStagingBufferSize,
            VertexStagingBuffer.Buffer
        );
        VertexStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, VertexStagingBufferSize, 0);
        VertexStagingBufferPtr = reinterpret_cast<uint8_t*>(VertexStagingBuffer.MappedMemory);
    }
    if (IndexBufferNeedsDefragmentation)
    {
        //Create a staging buffer big enough to cover all the indices 
        VKCORE::VirtualArenaAllocator IndexStagingBufferAllocator(IndexStagingBufferSize);
        VKCORE::CreateStagingBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            IndexStagingBufferSize,
            IndexStagingBuffer.Buffer
        );
        IndexStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, IndexStagingBufferSize, 0);
        IndexStagingBufferPtr = reinterpret_cast<uint8_t*>(IndexStagingBuffer.MappedMemory);
    }
    if (IndirectBufferNeedsDefragmentation)
    {
        //Create a staging buffer big enough to cover all the indirect commands 
        VKCORE::VirtualArenaAllocator IndirectStagingBufferAllocator(IndirectStagingBufferSize);
        VKCORE::CreateStagingBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            IndirectStagingBufferSize,
            IndirectStagingBuffer.Buffer
        );
        IndirectStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, IndirectStagingBufferSize, 0);
        IndirectStagingBufferPtr = reinterpret_cast<uint8_t*>(IndirectStagingBuffer.MappedMemory);
    }

    //Defragmenting the buffer allocators
    VertexBufferAllocator.Defragment(IndirectCommandAllocatedRegions);
    IndexBufferAllocator.Defragment(IndirectCommandAllocatedRegions);
    IndirectBufferAllocator.Defragment(IndirectCommandAllocatedRegions);
   
    uint32_t CurrentMesh = 0;
    //Produce copy informations
    std::vector<VKCORE::BufferCopyInfo> CopyInfos(3);
    for (auto& [ModelPtr, ModelEntryMetaData] : CurrentFrameModelEntries.ModelEntries)
    {
        for (auto& MeshEntry : ModelEntryMetaData.MeshEntries)
        {
            auto& MeshEntryPtr = MeshEntry.MeshPtr;
            auto& MeshEntryMetaData = MeshEntry.MetaData;
            if (VertexStagingBufferPtr && VertexBufferNeedsDefragmentation)
            {
                const VKCORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[0];
                memcpy(VertexStagingBufferPtr + AllocatedRegion.Offset, MeshEntryPtr->Vertices.data(), AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = AllocatedRegion.Offset;
                CopyRegion.size = AllocatedRegion.Size;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[0].CopyRegions.push_back(CopyRegion);

                MeshEntryMetaData.DrawInfo.VertexOffset = AllocatedRegion.Offset / sizeof(Vertex3D);
            }
            if (IndexStagingBufferPtr && IndexBufferNeedsDefragmentation)
            {
                const VKCORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[1];
                memcpy(IndexStagingBufferPtr + AllocatedRegion.Offset, MeshEntryPtr->Indices.data(), AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = AllocatedRegion.Offset;
                CopyRegion.size = AllocatedRegion.Size;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[1].CopyRegions.push_back(CopyRegion);

                MeshEntryMetaData.DrawInfo.FirstIndex = AllocatedRegion.Offset / sizeof(uint32_t);
            }
            if (IndirectStagingBufferPtr && (VertexBufferNeedsDefragmentation || IndexBufferNeedsDefragmentation || IndirectBufferNeedsDefragmentation))
            {
                ExtendedIndirectCommand NewIndirectCommand{};
                NewIndirectCommand.IndexCount = MeshEntryMetaData.DrawInfo.IndexCount;
                NewIndirectCommand.FirstIndex = MeshEntryMetaData.DrawInfo.FirstIndex;
                NewIndirectCommand.InstanceCount = ModelEntryMetaData.MetaData.ReferenceCount;
                NewIndirectCommand.VertexOffset = MeshEntryMetaData.DrawInfo.VertexOffset;
                NewIndirectCommand.MeshIndex = ModelEntryMetaData.MetaData.Index;
                NewIndirectCommand.FirstInstance = CurrentMesh;

                const VKCORE::MemoryRegion& AllocatedRegion = MeshEntryMetaData.MemoryRegions[2];
                memcpy(IndirectStagingBufferPtr + AllocatedRegion.Offset, &NewIndirectCommand, AllocatedRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = AllocatedRegion.Offset;
                CopyRegion.size = AllocatedRegion.Size;
                CopyRegion.srcOffset = AllocatedRegion.Offset;
                CopyInfos[2].CopyRegions.push_back(CopyRegion);
            }
            CurrentMesh++;
        }
    }
    CopyInfos[VERTEX_COPY].SourceBuffer = VertexStagingBuffer.Buffer.BufferObject;
    CopyInfos[VERTEX_COPY].DestinationBuffer = VertexBuffer.Buffer.BufferObject;
    CopyInfos[INDEX_COPY].SourceBuffer = IndexStagingBuffer.Buffer.BufferObject;
    CopyInfos[INDEX_COPY].DestinationBuffer = IndexBuffer.Buffer.BufferObject;
    CopyInfos[INDIRECT_COPY].SourceBuffer = IndirectStagingBuffer.Buffer.BufferObject;
    CopyInfos[INDIRECT_COPY].DestinationBuffer = IndirectBuffer.Buffer.BufferObject;

    //Execute copy operations 
    VKCORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    //Destroy the staging buffers
    VertexStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    IndexStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    IndirectStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::MeshManager::WriteTexture(
    uint32_t TextureTypeIndex,
    VKSCENE::Mesh& Mesh,
    VKSCENE::TextureImportManager& TextureImportManager,
    std::vector<VKCORE::DescriptorSetWriteImage>& ImageWrites,
    std::vector<int>& TextureIndexes,
    int& CurrentImageIndex,
    VkDescriptorSet DestinationDescriptorSet
)
{
    auto TextureIndex = Mesh.MeshMaterial.GetTexture(static_cast<MaterialTextureType>(TextureTypeIndex));
    if (TextureIndex)
    {
        auto& Iterator = TextureImportManager.TextureDatas.find(TextureIndex);
        if (Iterator == TextureImportManager.TextureDatas.end())
        {
            TextureIndexes.push_back(-1);
            return;
        }
        auto& TextureData = TextureImportManager.TextureDatas[TextureIndex];

        VKCORE::DescriptorSetWriteImage NewTextureWrite(
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

void VKSCENE::MeshManager::UpdateTextureDescriptors(MeshTextureUpdateInfo Info)
{
    auto FrameIndex = Info.FrameIndex;
    auto& TextureImportManager = *Info.TextureImportManagerPtr;
    auto& DescriptorSets = Info.TargetDescriptorSets;

    auto& CurrentDescriptorSet = DescriptorSets[FrameIndex];

    std::vector<VKCORE::DescriptorSetWriteImage> ImageWrites;
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

    auto& CurrentTextureIndexBuffer = PerformanceModeBuffers.TexturesIndexBuffers[FrameIndex];
    auto& TextureIndexBufferAllocator = CurrentTextureIndexBuffer.Allocator;
    auto OldCapacity = TextureIndexBufferAllocator.GetCapacity();
    TextureIndexBufferAllocator.Reset();
    VkDeviceSize BufferSize = sizeof(int) * TextureIndexes.size();
    TextureIndexBufferAllocator.Allocate(BufferSize);

    std::vector<VKCORE::DescriptorSetWriteBuffer> BufferWrites;
    if (TextureIndexBufferAllocator.GetCapacity() > OldCapacity)
    {
        CurrentTextureIndexBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TextureIndexBufferAllocator.GetCapacity(),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CurrentTextureIndexBuffer.Buffer
        );

        VKCORE::DescriptorSetWriteBuffer DrawMetaDataBufferWrite(
            CurrentTextureIndexBuffer.Buffer,
            TextureIndexBufferAllocator.GetCapacity(),
            1,
            CurrentDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        BufferWrites.push_back(DrawMetaDataBufferWrite);
    }

    VKCORE::WriteDescriptorSets(
        RendererContext->DeviceContext.logicalDevice,
        BufferWrites,
        ImageWrites
    );

    VKCORE::PersistentBuffer TextureIndexStagingBuffer;
    VKCORE::CreateStagingBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        TextureIndexBufferAllocator.GetCapacity(),
        TextureIndexStagingBuffer.Buffer
    );
    TextureIndexStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0,TextureIndexBufferAllocator.GetCapacity(), 0);

    memcpy(TextureIndexStagingBuffer.MappedMemory, TextureIndexes.data(), BufferSize);
  
    VKCORE::CopyBuffer(
        TextureIndexStagingBuffer.Buffer.BufferObject,
        CurrentTextureIndexBuffer.Buffer.BufferObject,
        BufferSize,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    TextureIndexStagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}
