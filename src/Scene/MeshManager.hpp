#include "Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"

namespace RENDERER
{
    class RendererContext;
    class Renderer;
}

namespace SCENE
{
    class TextureImportManager;
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
    struct ImportTarget
    {
        ModelHandle* ConsumerModel;
        std::vector<GeometryData> GeometryDatas;
        std::vector<size_t> GeometryHandles;
    };

    class MeshManager : COMMON::Destructible
    {
        friend class SceneMeshManager;
        friend class RENDERER::Renderer;
    public:
        MeshManager(TextureImportManager& ImportManager, RENDERER::RendererContext& RendererContext);
        MeshManager() = default;
        void Create(TextureImportManager& ImportManager, RENDERER::RendererContext& RendererContext);

        void Destroy() override;

        void AppendImportTask(ModelImportInfo ImportInfo);
        void SubmitImport();
        void WaitImportIdle();
        std::vector<std::future<void>> Futures;
        std::vector<ModelImportInfo> ImportQueue;
        double StartingTime;

        RENDERER_CORE::BufferAllocator& GetCurrentVertexBuffer(uint32_t FrameIndex);
        RENDERER_CORE::BufferAllocator& GetCurrentIndexBuffer(uint32_t FrameIndex);
    private:
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

        //Flags to let import targets be appended in the geometry buffers
        std::array<bool, MAX_FRAMES_IN_FLIGHT> AppendList;
        std::array<std::vector<size_t>, MAX_FRAMES_IN_FLIGHT> EraseList;

        //Storage unit that links models with the mesh IDs
        std::unordered_map<std::string, std::vector<size_t>> ModelMeshLinks;

        //Import targets to store imported geometry. CPU side data is to be erased so cleanses per import
        std::vector<ImportTarget> ImportTargets;

        //Main storage unit for geometry allocation information
        std::array<std::unordered_map<size_t, GeometryEntry>,MAX_FRAMES_IN_FLIGHT> GeometryEntries;

        TextureImportManager* ImportManager = nullptr;
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