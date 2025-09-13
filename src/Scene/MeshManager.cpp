#include "MeshManager.hpp"
#include "MaterialManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "../Renderer/RendererContext.hpp"

SCENE::MeshManager::MeshManager(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext)
{
    Create(ImportManager, RendererContext);
}

void SCENE::MeshManager::Create(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext)
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VertexBuffers[i].Create();
        IndexBuffers[i].Create();

        VertexBufferSet[i] = false;
        IndexBufferSet[i] = false;
    }
    this->ImportManager = &ImportManager;
    this->RendererContext = &RendererContext;

    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);
}

void SCENE::MeshManager::Destroy()
{
    if (IsDestroyed || !RendererContext) return;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        GeometryEntries[i].clear();
    }
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT * 2; i++)
    {
        VertexBuffers[i].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
        IndexBuffers[i].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
    }

    MeshImportResults.clear();
    ImportQueue.clear();
    Futures.clear();

    IsDestroyed = true;
    std::cout << "Mesh manager destroyed!" << std::endl;
}

void SCENE::MeshManager::AppendImportTask(ModelImportInfo ImportInfo)
{
    ImportQueue.push_back(ImportInfo);
}

void SCENE::MeshManager::SubmitImport()
{
    StartingTime = glfwGetTime();
    for (size_t i = 0; i < ImportQueue.size(); i++)
    {
        auto& Import = ImportQueue[i];
        Futures.push_back({
            std::async(std::launch::async, SCENE::Import3DGeometry, Import.ModelFilePath, std::ref(*ImportManager)),
            Import.DestinationModelHandle
        });
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Submitted model import [" + std::string(Import.ModelFilePath) + "].");
    }
    ImportQueue.clear();
}

void SCENE::MeshManager::WaitImportIdle()
{
    //Handle the fresh imports
    std::vector<MeshImportResult> NewImportResults;
    NewImportResults.reserve(Futures.size());
    for (size_t i = 0; i < Futures.size(); i++)
    {
        auto& Future = Futures[i];
        try {
            //Creates a new mesh import result which will temporarily keep the geometry datas and the model linkage
            auto GeometryData = Future.first.get();
            MeshImportResult NewImportResult{};
            NewImportResult.ConsumerModel = Future.second;
            NewImportResult.GeometryDatas = GeometryData;
            NewImportResults.push_back(std::move(NewImportResult));
        }
        catch (const std::exception& e) {
            LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR,
                std::string("Mesh import failed: ") + e.what());
        }
    }
    Futures.clear();

    //Fill in consumer model and required handles
    for (auto& NewImportResult : NewImportResults)
    {
        NewImportResult.GeometryHandles.reserve(NewImportResult.GeometryDatas.size());

        for (size_t y = 0; y < NewImportResult.GeometryDatas.size(); y++)
        {
            MeshHandle NewMeshHandle{};
            NewMeshHandle.GeometryID = GenerateResourceID();
            NewImportResult.GeometryHandles.push_back(NewMeshHandle.GeometryID);
            NewImportResult.ConsumerModel->Meshes.push_back(NewMeshHandle);
        }
        //Set to-be processed by the frames
        NewImportResult.ResetFlags();
        //Append in the actual result vector
        MeshImportResults.push_back(std::move(NewImportResult));
    }
    NewImportResults.clear();

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Models were imported in: " << DeltaTime << " seconds" << std::endl;
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Models were imported in " + std::to_string(DeltaTime) + " seconds.");
}

RENDERER_CORE::BufferAllocator& SCENE::MeshManager::GetCurrentVertexBuffer(uint32_t FrameIndex)
{
    return VertexBuffers[VertexBufferSet[FrameIndex] * MAX_FRAMES_IN_FLIGHT + FrameIndex];
}

RENDERER_CORE::BufferAllocator& SCENE::MeshManager::GetCurrentIndexBuffer(uint32_t FrameIndex)
{
    return IndexBuffers[IndexBufferSet[FrameIndex] * MAX_FRAMES_IN_FLIGHT + FrameIndex];
}

void SCENE::MeshManager::UpdateGeometryEntries(uint32_t FrameIndex)
{
    if (MeshImportResults.empty()) return;

    auto& VertexBufferSetBit = VertexBufferSet[FrameIndex];
    auto& IndexBufferSetBit = IndexBufferSet[FrameIndex];

    auto& VertexBuffer = VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex];
    auto& IndexBuffer = IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex];
    auto& GeometryEntryList = GeometryEntries[FrameIndex];

    size_t OldVertexCapacity = VertexBuffer.Allocator.GetCapacity();
    size_t OldIndexCapacity = IndexBuffer.Allocator.GetCapacity();
    size_t VertexSize, IndexSize,VertexBufferSize = 0, IndexBufferSize = 0;

    std::unordered_map<size_t,GeometryEntry> InsertedGeometryEntries;
    std::unordered_map<size_t,GeometryData*> GeometryDataReferences;
    for (auto& ImportResult : MeshImportResults)
    {
        for (size_t i = 0; i < ImportResult.GeometryDatas.size(); i++)
        {
            GeometryData& Data = ImportResult.GeometryDatas[i];
            size_t& Handle = ImportResult.GeometryHandles[i];

            //Referencing the data to use for copying later on
            GeometryDataReferences[Handle] = &ImportResult.GeometryDatas[i];

            //Detect the data sizes and append them in the overall inserted size
            VertexSize = Data.Vertices.size() * sizeof(Vertex3D);
            IndexSize = Data.Indices.size() * sizeof(uint32_t);
            VertexBufferSize += VertexSize;
            IndexBufferSize += IndexSize;

            //Fill in the new geometry entry
            GeometryEntry NewGeometryEntry{};
            NewGeometryEntry.MeshMaterial = Data.MeshMaterial;
            NewGeometryEntry.BoundingBox = Data.BoundingBox;

            NewGeometryEntry.VertexRegion = VertexBuffer.Allocator.Suballocate(VertexSize);
            NewGeometryEntry.IndexRegion = IndexBuffer.Allocator.Suballocate(IndexSize);
            //GeometryEntryList[Handle] = NewGeometryEntry;
            InsertedGeometryEntries[Handle] = NewGeometryEntry;
        }
        //Set processed by the current frame
        ImportResult.SetFlag(FrameIndex, true);
    }

    //Decide whether buffers should be reallocated or not
    bool VertexBufferReallocated = VertexBuffer.Allocator.GetCapacity() > OldVertexCapacity;
    bool IndexBufferReallocated = IndexBuffer.Allocator.GetCapacity() > OldIndexCapacity;
    bool VertexBufferAllocatedFirstTime = !OldVertexCapacity;
    bool IndexBufferAllocatedFirstTime = !OldIndexCapacity;

    size_t VertexStagingBufferSize = VertexBufferSize,
           IndexStagingBufferSize = IndexBufferSize;

    //Create or recreate geometry buffers
    RENDERER_CORE::PersistentBufferAllocator StagingBuffer{};
    StagingBuffer.Allocator.Create(VertexStagingBufferSize + IndexStagingBufferSize);
    CreateGeometryBuffers(
        RendererContext, 
        StagingBuffer,
        VertexBufferReallocated, 
        IndexBufferReallocated,
        VertexBufferAllocatedFirstTime,
        IndexBufferAllocatedFirstTime,
        FrameIndex
    );
    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);
    if (!StagingBufferPtr) return;

    std::vector<RENDERER_CORE::BufferCopyInfo> CopyInfos(2);
    //Copy the existing data from the old buffers to the new buffers
    HandleGeometryBufferReallocationCopy(
        CopyInfos,
        VertexBufferReallocated,
        IndexBufferReallocated,
        OldVertexCapacity,
        OldIndexCapacity,
        FrameIndex
    );

    //Append newly inserted data
    for (auto& [Handle,GeoEntry] : InsertedGeometryEntries)
    {
        GeometryData*& Data = GeometryDataReferences[Handle];

        RENDERER_CORE::MemoryRegion VertexAllocatedRegion = StagingBuffer.Allocator.Suballocate(GeoEntry.VertexRegion.Size);
        memcpy(StagingBufferPtr + VertexAllocatedRegion.Offset, Data->Vertices.data(), VertexAllocatedRegion.Size);

        VkBufferCopy VertexCopyRegion{};
        VertexCopyRegion.dstOffset = GeoEntry.VertexRegion.Offset;
        VertexCopyRegion.size = VertexAllocatedRegion.Size;
        VertexCopyRegion.srcOffset = VertexAllocatedRegion.Offset;
        CopyInfos[0].CopyRegions.push_back(VertexCopyRegion);

        RENDERER_CORE::MemoryRegion IndexAllocatedRegion = StagingBuffer.Allocator.Suballocate(GeoEntry.IndexRegion.Size);
        memcpy(StagingBufferPtr + IndexAllocatedRegion.Offset, Data->Indices.data(), IndexAllocatedRegion.Size);

        VkBufferCopy IndexCopyRegion{};
        IndexCopyRegion.dstOffset = GeoEntry.IndexRegion.Offset;
        IndexCopyRegion.size = IndexAllocatedRegion.Size;
        IndexCopyRegion.srcOffset = IndexAllocatedRegion.Offset;
        CopyInfos[1].CopyRegions.push_back(IndexCopyRegion);
    }
    CopyInfos[0].SourceBuffer = StagingBuffer.Buffer.Buffer.BufferObject;
    CopyInfos[0].DestinationBuffer = VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;
    CopyInfos[1].SourceBuffer = StagingBuffer.Buffer.Buffer.BufferObject;
    CopyInfos[1].DestinationBuffer = IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;

    RENDERER_CORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.LogicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    StagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);

    for (auto& [handle, entry] : InsertedGeometryEntries)
        GeometryEntryList[handle] = std::move(entry);

    //Clear out the fully processed targets
    for (size_t i = 0; i < MeshImportResults.size(); i++)
    {
        auto& Target = MeshImportResults[i];
        if (Target.IsProcessedByAll())
        {
            std::swap(Target, MeshImportResults.back());
            MeshImportResults.pop_back();
            --i;
        }
    }
}

void SCENE::MeshManager::CreateGeometryBuffers(
    RENDERER::RendererContext* RendererContext,
    RENDERER_CORE::PersistentBufferAllocator &StagingBuffer,
    bool VertexBufferReallocated,
    bool IndexBufferReallocated,
    bool VertexBufferAllocatedFirstTime,
    bool IndexBufferAllocatedFirstTime,
    uint32_t FrameIndex
)
{
    auto& VertexBufferSetBit = VertexBufferSet[FrameIndex];
    auto& IndexBufferSetBit = IndexBufferSet[FrameIndex];

    if (VertexBufferReallocated)
    {
        size_t BufferSize = VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator.GetCapacity();

        SCENE::RecreateBuffer(
            RendererContext,
            BufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer
        );

        //Allocate the second buffer in case copying needed
        if (!VertexBufferAllocatedFirstTime)
        {
            SCENE::RecreateBuffer(
                RendererContext,
                BufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VertexBuffers[!VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer
            );

            VertexBuffers[!VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator = VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator;
        }
    }
    if (IndexBufferReallocated)
    {
        size_t BufferSize = IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator.GetCapacity();

        SCENE::RecreateBuffer(
            RendererContext,
            BufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer
        );

        //Allocate the second buffer in case copying needed
        if (!IndexBufferAllocatedFirstTime)
        {
            SCENE::RecreateBuffer(
                RendererContext,
                BufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                IndexBuffers[!IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer
            );
            IndexBuffers[!IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator = IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator;
        }
    }

    RENDERER_CORE::CreateStagingBuffer(
        RendererContext->DeviceContext.PhysicalDevice,
        RendererContext->DeviceContext.LogicalDevice,
        StagingBuffer.Allocator.GetCapacity(),
        StagingBuffer.Buffer.Buffer
    );
    StagingBuffer.Buffer.Map(RendererContext->DeviceContext.LogicalDevice, 0, StagingBuffer.Allocator.GetCapacity(), 0);
}

void SCENE::MeshManager::HandleGeometryBufferReallocationCopy(
    std::vector<RENDERER_CORE::BufferCopyInfo> &CopyInfos,
    bool VertexBufferReallocated,
    bool IndexBufferReallocated,
    size_t VertexCapacity,
    size_t IndexCapacity,
    uint32_t FrameIndex
)
{
    //If both buffers are allocated for the first time no need to copy 
    if (!VertexCapacity && !IndexCapacity) return;

    if (VertexBufferReallocated || IndexBufferReallocated)
    {
        auto& VertexBufferSetBit = VertexBufferSet[FrameIndex];
        auto& IndexBufferSetBit = IndexBufferSet[FrameIndex];

        //Create block copy infos
        if (VertexBufferReallocated && VertexCapacity)
        {
            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = 0;
            CopyRegion.size = VertexCapacity;
            CopyRegion.srcOffset = 0;

            CopyInfos[0].CopyRegions.push_back(CopyRegion);
            CopyInfos[0].SourceBuffer = VertexBuffers[VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;
            CopyInfos[0].DestinationBuffer = VertexBuffers[!VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;
            VertexBufferSetBit = !VertexBufferSetBit;
        }
        if (IndexBufferReallocated && IndexCapacity)
        {
            VkBufferCopy CopyRegion{};
            CopyRegion.dstOffset = 0;
            CopyRegion.size = IndexCapacity;
            CopyRegion.srcOffset = 0;
            CopyInfos[1].CopyRegions.push_back(CopyRegion);
            CopyInfos[1].SourceBuffer = IndexBuffers[IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;
            CopyInfos[1].DestinationBuffer = IndexBuffers[!IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.BufferObject;
            IndexBufferSetBit = !IndexBufferSetBit;
        }

        RENDERER_CORE::CopyBuffer(
            CopyInfos,
            RendererContext->DeviceContext.LogicalDevice,
            RendererContext->CommandPool.commandPool,
            RendererContext->DeviceContext.GraphicsQueue
        );

        //Clean up the old buffers
        if (VertexBufferReallocated && VertexCapacity)
        {
            VertexBuffers[!VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
            VertexBuffers[!VertexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator.Reset();
        }
        if (IndexBufferReallocated && IndexCapacity)
        {
            IndexBuffers[!IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
            IndexBuffers[!IndexBufferSetBit * MAX_FRAMES_IN_FLIGHT + FrameIndex].Allocator.Reset();
        }

        CopyInfos[0].CopyRegions.clear();
        CopyInfos[1].CopyRegions.clear();
    }
}

void SCENE::RecreateBuffer(
    RENDERER::RendererContext* RendererContext,
    VkDeviceSize NewCapacity,
    VkBufferUsageFlags Usage, 
    VkMemoryPropertyFlags Properties,
    RENDERER_CORE::Buffer& Buffer
)
{
    Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
    CreateBuffer(
        RendererContext->DeviceContext.PhysicalDevice,
        RendererContext->DeviceContext.LogicalDevice,
        NewCapacity,
        Usage,
        Properties,
        Buffer
    );
}

bool SCENE::MeshImportResult::IsProcessedByAll()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (!ProcessedPerFrame[i]) return false;
    }
    return true;
}

void SCENE::MeshImportResult::ResetFlags()
{
    ProcessedPerFrame.fill(false);
}

void SCENE::MeshImportResult::SetFlag(uint32_t FrameIndex, bool Value)
{
    ProcessedPerFrame[FrameIndex] = Value;
}
