#pragma once
#include "Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"

namespace RENDERER
{
    class RendererContext;
    class Renderer;
    class DeferredRenderPipeline;
}

namespace SCENE
{
    class TextureManager;
    class SceneMeshManager;

    //Info used during model importing
    struct ModelImportInfo
    {
        //SCENE::Model3D* DestinationModel;
        SCENE::ModelHandle* DestinationModelHandle;
        const char* ModelFilePath;
    };

    //Geometry entry data keeping the references to the geometry buffers
    struct GeometryEntry
    {
        RENDERER_CORE::MemoryRegion VertexRegion;
        RENDERER_CORE::MemoryRegion IndexRegion;
        BoundingBoxAABB BoundingBox;
        Material MeshMaterial;
    };

    //Intermediate import data target.
    struct MeshImportResult
    {
        MeshImportResult() { ProcessedPerFrame.fill(true); };

        ModelHandle* ConsumerModel = nullptr;
        std::vector<GeometryData> GeometryDatas;
        std::vector<size_t> GeometryHandles;
        //Flags to keep track of whether the target is included in the entries or not. 
        //If all frames had previously processed the target, it's erased 
        std::array<bool, MAX_FRAMES_IN_FLIGHT> ProcessedPerFrame;

        //Is the target processed by the frame with the given index
        bool IsProcessedBy(uint32_t FrameIndex) { return ProcessedPerFrame[FrameIndex]; }
        bool IsProcessedByAll();
        void ResetFlags();
        void SetFlag(uint32_t FrameIndex,bool Value);
    };

    class MeshManager : COMMON::Destructible
    {
        friend class SceneMeshManager;
        friend class RENDERER::Renderer;
        friend class RENDERER::DeferredRenderPipeline;
    public:
        MeshManager(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext);
        MeshManager() = default;
        void Create(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext);

        void Destroy() override;

        //Appends an import task with the information.
        void AppendImportTask(ModelImportInfo ImportInfo);
        //Submits the existing import tasks asynchronously.
        //Although it's recommended to submit all import tasks at once, 
        //it's possible to submit tasks multiple times before existing import tasks in flight conclude.
        void SubmitImport();
        //Waits for the import tasks in flight and processed the result.
        //Creates intermediary import results to be processed further.
        void WaitImportIdle();

        using ImportFuture = std::pair<std::future<std::vector<SCENE::GeometryData>>, SCENE::ModelHandle*>;
        std::vector<ImportFuture> Futures;
        std::vector<ModelImportInfo> ImportQueue;
        double StartingTime;

        RENDERER_CORE::BufferAllocator& GetCurrentVertexBuffer(uint32_t FrameIndex);
        RENDERER_CORE::BufferAllocator& GetCurrentIndexBuffer(uint32_t FrameIndex);
    private:
        //Processes the dwelling import results (the intermediary product of the importing process).
        //It processes the geometry data kept in the import results and uploads them on the buffers to be used by the (graphics) device.
        //Also creates geometry entries which keep the meta data of the uploaded geometries. 
        //These entries then are used in the scene-Model Instance linking process.
        //Which also means that scenes are closely tied with their respective geometry managers and cannot be switched past their constructions.
        //Although it's not expected to have multiple mesh managers around, it's possible.
        //This function is already called internally within the scenes so there is no need to call it explicitly. 
        void UpdateGeometryEntries(uint32_t FrameIndex);

        void CreateGeometryBuffers(
            RENDERER::RendererContext* RendererContext,
            RENDERER_CORE::PersistentBufferAllocator& StagingBuffer,
            bool VertexBufferReallocated,
            bool IndexBufferReallocated,
            bool VertexBufferAllocatedFirstTime,
            bool IndexBufferAllocatedFirstTime,
            uint32_t FrameIndex
        );

        void HandleGeometryBufferReallocationCopy(
            std::vector<RENDERER_CORE::BufferCopyInfo>& CopyInfos,
            bool VertexBufferReallocated,
            bool IndexBufferReallocated,
            size_t VertexCapacity,
            size_t IndexCapacity,
            uint32_t FrameIndex
        );

        //Central Geometry Buffers
        std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT * 2> VertexBuffers;
        std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT * 2> IndexBuffers;

        std::array<bool, MAX_FRAMES_IN_FLIGHT> VertexBufferSet;
        std::array<bool, MAX_FRAMES_IN_FLIGHT> IndexBufferSet;

        std::array<std::vector<size_t>, MAX_FRAMES_IN_FLIGHT> EraseList;

        //Storage unit that links models with the mesh IDs
        std::unordered_map<std::string, std::vector<size_t>> ModelMeshLinks;

        //Import targets to store imported geometry. CPU side data is to be erased so cleanses per import
        std::vector<MeshImportResult> MeshImportResults;

        //Main storage unit for geometry allocation information
        std::array<std::unordered_map<size_t, GeometryEntry>,MAX_FRAMES_IN_FLIGHT> GeometryEntries;

        TextureManager* ImportManager = nullptr;
        RENDERER::RendererContext* RendererContext = nullptr;
    };

    //Destroys and creates the given buffer
    void RecreateBuffer(
        RENDERER::RendererContext *RendererContext,
        VkDeviceSize NewCapacity,
        VkBufferUsageFlags Usage,
        VkMemoryPropertyFlags Properties,
        RENDERER_CORE::Buffer& Buffer
    );
}