#include "MeshManager.hpp"
#include "MaterialManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Renderer/ResourceManager.hpp"

RENDERER::MeshManager::MeshManager(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr)
{
    Create(ImportManager, RendererContext, ResourceManagerPtr);
}

void RENDERER::MeshManager::Create(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr)
{
    this->ImportManager = &ImportManager;
    this->RendererContext = &RendererContext;
    this->ResourceManagerPtr = &ResourceManagerPtr;

    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::MeshManager::Destroy()
{
    if (IsDestroyed || !RendererContext) return;

    for (size_t j = 0; j < GeometryBufferPages.size(); j++)
    {
        RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, GeometryBufferPages[j].Buffer);
    }
    GeometryEntries.clear();

    MeshImportResults.clear();
    ImportQueue.clear();
    Futures.clear();

    IsDestroyed = true;
    LOG_CONSOLE(COMMON::LOG_SEVERITY_INFO, "Mesh manager destroyed!");
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Mesh manager destroyed!");
}

void RENDERER::MeshManager::AppendImportTask(ModelImportInfo ImportInfo)
{
    ImportQueue.push_back(ImportInfo);
}

void RENDERER::MeshManager::SubmitImport()
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

void RENDERER::MeshManager::WaitImportsIdle()
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
            SCENE::GeometryData& GeometryData = NewImportResult.GeometryDatas[y];

            SCENE::MeshHandle NewMeshHandle{};
            NewMeshHandle.MeshMaterial = GeometryData.MeshMaterial;
            NewMeshHandle.BoundingBox = GeometryData.BoundingBox;
            NewMeshHandle.GeometryID = COMMON::GenerateHandleID();

            NewImportResult.GeometryHandles.push_back(NewMeshHandle.GeometryID);
            NewImportResult.ConsumerModel->Meshes.push_back(std::move(NewMeshHandle));
        }
        //Append in the actual result vector
        MeshImportResults.push_back(std::move(NewImportResult));
    }
    NewImportResults.clear();

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Models were imported in: " << DeltaTime << " seconds" << std::endl;
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Models were imported in " + std::to_string(DeltaTime) + " seconds.");
}

void RENDERER::MeshManager::UpdateGeometryEntries()
{
    if (MeshImportResults.empty()) return;
   
    auto& GeometryEntryList = GeometryEntries;
    auto& GeometryBuffers = GeometryBufferPages;

    size_t VertexSize, IndexSize, VertexBufferSize = 0, IndexBufferSize = 0;

    std::unordered_map<size_t, GeometryEntry> InsertedGeometryEntries;
    std::unordered_map<size_t, SCENE::GeometryData*> GeometryDataReferences;
    std::vector<std::shared_ptr<COMMON::AsyncToken>> UploadedTokens;
    UploadedTokens.reserve(MeshImportResults.size() + 1);
    for (auto& ImportResult : MeshImportResults)
    {
        for (size_t i = 0; i < ImportResult.GeometryDatas.size(); i++)
        {
            SCENE::GeometryData& Data = ImportResult.GeometryDatas[i];
            size_t& Handle = ImportResult.GeometryHandles[i];

            //Referencing the data to use for copying later on
            GeometryDataReferences[Handle] = &ImportResult.GeometryDatas[i];

            //Detect the data sizes and append them in the overall inserted size
            VertexSize = Data.Vertices.size() * sizeof(SCENE::Vertex3D);
            IndexSize = Data.Indices.size() * sizeof(uint32_t);

            std::shared_ptr<COMMON::AsyncToken> NewToken = std::make_shared<COMMON::AsyncToken>();

            //Fill in the new geometry entry
            GeometryEntry NewGeometryEntry{};
            NewGeometryEntry.MeshMaterial = Data.MeshMaterial;
            NewGeometryEntry.BoundingBox = Data.BoundingBox;
            NewGeometryEntry.Uploaded = NewToken;

            UploadedTokens.push_back(std::move(NewToken));

            RENDERER::BufferPageAllocationInfo AllocationInfo = AllocateFromGeometryBuffers(VertexSize, IndexSize);
            NewGeometryEntry.VertexRegion = AllocationInfo.VertexRegion;
            NewGeometryEntry.IndexRegion = AllocationInfo.IndexRegion;
            NewGeometryEntry.PageIndex = AllocationInfo.PageIndex;
            //GeometryEntryList[Handle] = NewGeometryEntry;
            InsertedGeometryEntries[Handle] = NewGeometryEntry;

            VertexBufferSize += AllocationInfo.VertexRegion.TotalConsumedSize;
            IndexBufferSize += AllocationInfo.IndexRegion.TotalConsumedSize;
        }
    }

    size_t VertexStagingBufferSize = VertexBufferSize,
        IndexStagingBufferSize = IndexBufferSize;
   
    //std::vector<uint8_t> CopiedData(VertexBufferSize + IndexBufferSize);
    uint8_t* DataBlockPtr = reinterpret_cast<uint8_t*>(malloc(VertexBufferSize + IndexBufferSize));

    std::shared_ptr<RENDERER::DataBlock> CopiedDataBlock = std::make_shared<RENDERER::DataBlock>();
    CopiedDataBlock->DataPtr = DataBlockPtr;
    CopiedDataBlock->SizeInBytes = VertexBufferSize + IndexBufferSize;
    CopiedDataBlock->Deleter = [DataBlockPtr]() { 
        free(DataBlockPtr); 
        LOG_CONSOLE(COMMON::LOG_SEVERITY_DEBUG, "Deleted the mesh data block");
    };
    size_t Offset = 0;

    std::vector<std::vector<VkBufferCopy>> CopyRegionLists(this->GeometryBufferPages.size());

    //Append newly inserted data
    for (auto& [Handle, GeoEntry] : InsertedGeometryEntries)
    {
        SCENE::GeometryData*& Data = GeometryDataReferences[Handle];
        std::vector<VkBufferCopy>& CopyRegions = CopyRegionLists[GeoEntry.PageIndex];

        VkBufferCopy VertexCopyRegion{};
        VertexCopyRegion.dstOffset = GeoEntry.VertexRegion.Offset;
        VertexCopyRegion.size = GeoEntry.VertexRegion.Size;
        VertexCopyRegion.srcOffset = Offset;
       
        CopyRegions.push_back(std::move(VertexCopyRegion));
        memcpy(CopiedDataBlock->DataPtr + VertexCopyRegion.srcOffset, Data->Vertices.data(), GeoEntry.VertexRegion.Size);
        Offset += GeoEntry.VertexRegion.Size;

        VkBufferCopy IndexCopyRegion{};
        IndexCopyRegion.dstOffset = GeoEntry.IndexRegion.Offset;
        IndexCopyRegion.size = GeoEntry.IndexRegion.Size;
        IndexCopyRegion.srcOffset = Offset;
       
        CopyRegions.push_back(std::move(IndexCopyRegion));
        memcpy(CopiedDataBlock->DataPtr + IndexCopyRegion.srcOffset, Data->Indices.data(), GeoEntry.IndexRegion.Size);
        Offset += GeoEntry.IndexRegion.Size;
    }
   
    GeometryBufferPageCopyTokens.clear();
    for (size_t i = 0; i < this->GeometryBufferPages.size(); i++)
    {
        RENDERER_CORE::BufferAllocator& PageBuffer = GeometryBufferPages[i];
        std::vector<VkBufferCopy>& CopyRegions = CopyRegionLists[i];
        if (!CopyRegions.empty())
        {
            std::shared_ptr<COMMON::AsyncToken> Token = std::make_shared<COMMON::AsyncToken>();
            UploadedTokens.push_back(Token);
            ResourceManagerPtr->RequestBufferCopyOperation(
                CopyRegions,
                RENDERER_CORE::QUEUE_TYPE_TRANSFER,
                &PageBuffer.Buffer,
                CopiedDataBlock,
                1,
                COPY_OPERATION_FLAG_NONE,
                nullptr,
                0,
                UploadedTokens.data(),
                UploadedTokens.size()
            );
            GeometryBufferPageCopyTokens.push_back(std::move(Token));
        }
    }

    //StagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
    for (auto& [handle, entry] : InsertedGeometryEntries)
        GeometryEntryList[handle] = std::move(entry);

    MeshImportResults.clear();
}

RENDERER::BufferPageAllocationInfo RENDERER::MeshManager::AllocateFromGeometryBuffers(
    size_t VertexSize, 
    size_t IndexSize
)
{
    size_t VertexAlignedSize = VertexSize;
    size_t IndexAlignedSize = IndexSize;
    size_t CombinedSize = IndexAlignedSize + VertexAlignedSize;
    BufferPageAllocationInfo AllocationInfo{};
    if (!CombinedSize) return AllocationInfo;

    auto& CurrentPages = GeometryBufferPages;
    size_t BufferSize = glm::max(GeometryBufferPageSize, CombinedSize);
    //Append a page in case there isn't any.
    if (CurrentPages.empty())
    {
        RENDERER_CORE::BufferAllocator Buffer{};
        Buffer.Allocator.Create(BufferSize);
        RENDERER_CORE::CreateBuffer(
            RendererContext->DeviceContext.PhysicalDevice,
            RendererContext->DeviceContext.LogicalDevice,
            BufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Buffer.Buffer
        );
        CurrentPages.push_back(std::move(Buffer));
    }
    for (size_t i = 0; i < CurrentPages.size(); i++)
    {
        auto& Page = CurrentPages[i];
        RENDERER_CORE::MemoryRegion AllocatedRegion = Page.Allocator.Suballocate(CombinedSize, sizeof(SCENE::Vertex3D), false);
        if (!AllocatedRegion.Size)
        {
            //In case there isn't enough space, append a new page.
            if (i == (CurrentPages.size() - 1))
            {
                RENDERER_CORE::BufferAllocator Buffer{};
                Buffer.Allocator.Create(BufferSize);
                RENDERER_CORE::CreateBuffer(
                    RendererContext->DeviceContext.PhysicalDevice,
                    RendererContext->DeviceContext.LogicalDevice,
                    BufferSize,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    Buffer.Buffer
                );
                CurrentPages.push_back(std::move(Buffer));  
            }
            continue;
        }

        //Allocate as a combined and aligned chunk of memory and then manually split them to ensure they reside in the same page.
        RENDERER_CORE::MemoryRegion VertexMemoryRegion{};
        VertexMemoryRegion.Alignment = AllocatedRegion.Alignment;
        VertexMemoryRegion.Offset = AllocatedRegion.Offset;
        VertexMemoryRegion.Size = VertexAlignedSize;
        VertexMemoryRegion.TotalConsumedSize = VertexAlignedSize + (AllocatedRegion.Offset - AllocatedRegion.OffsetWithoutPadding);
        VertexMemoryRegion.OffsetWithoutPadding = AllocatedRegion.OffsetWithoutPadding;
        RENDERER_CORE::MemoryRegion IndexMemoryRegion{};
        IndexMemoryRegion.Alignment = AllocatedRegion.Alignment;
        IndexMemoryRegion.Offset = AllocatedRegion.Offset + VertexAlignedSize;
        IndexMemoryRegion.Size = IndexAlignedSize;
        IndexMemoryRegion.OffsetWithoutPadding = IndexMemoryRegion.Offset;
        IndexMemoryRegion.TotalConsumedSize = AllocatedRegion.Size - VertexAlignedSize;

        //Construct allocation info
        AllocationInfo.IndexRegion = std::move(IndexMemoryRegion);
        AllocationInfo.VertexRegion = std::move(VertexMemoryRegion);
        AllocationInfo.PageIndex = i;
        break;
    }
    return AllocationInfo;
}

void RENDERER::RecreateBuffer(
    RENDERER::RendererContext* RendererContext,
    VkDeviceSize NewCapacity,
    VkBufferUsageFlags Usage, 
    VkMemoryPropertyFlags Properties,
    RENDERER_CORE::Buffer& Buffer
)
{
    RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, Buffer);
    //Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
    CreateBuffer(
        RendererContext->DeviceContext.PhysicalDevice,
        RendererContext->DeviceContext.LogicalDevice,
        NewCapacity,
        Usage,
        Properties,
        Buffer
    );
}

