#pragma once
#include "../Scene/Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/AsyncToken.hpp"

namespace SCENE
{
    class SceneMeshManager;
}

namespace RENDERER
{
    class TextureManager;
    class RendererContext;
    class ResourceManager;
    class Renderer;
    class DeferredRenderPipeline;

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
        SCENE::BoundingBoxAABB BoundingBox;
        SCENE::Material MeshMaterial;
        uint32_t PageIndex;

        std::shared_ptr<COMMON::AsyncToken> Uploaded;
    };

    struct BufferPageAllocationInfo
    {
        uint32_t PageIndex = 0;
        RENDERER_CORE::MemoryRegion VertexRegion;
        RENDERER_CORE::MemoryRegion IndexRegion;
    };

    //Intermediate import data target.
    struct MeshImportResult
    {
        SCENE::ModelHandle* ConsumerModel = nullptr;
        std::vector<SCENE::GeometryData> GeometryDatas;
        std::vector<size_t> GeometryHandles;
    };

    class MeshManager : COMMON::Destructible
    {
        friend class SCENE::SceneMeshManager;
        friend class RENDERER::Renderer;
        friend class RENDERER::DeferredRenderPipeline;
    public:
        MeshManager(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr);
        MeshManager() = default;
        void Create(TextureManager& ImportManager, RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr);

        void Destroy() override;

        //Appends an import task with the information.
        void AppendImportTask(ModelImportInfo ImportInfo);
        //Submits the existing import tasks asynchronously.
        //Although it's recommended to submit all import tasks at once, 
        //it's possible to submit tasks multiple times before existing import tasks in flight conclude.
        void SubmitImport();
        //Waits for the import tasks in flight and processed the result.
        //Creates intermediary import results to be processed further.
        void WaitImportsIdle();

        using ImportFuture = std::pair<std::future<std::vector<SCENE::GeometryData>>, SCENE::ModelHandle*>;
        std::vector<ImportFuture> Futures;
        std::vector<ModelImportInfo> ImportQueue;
        double StartingTime;

        std::vector<std::shared_ptr<COMMON::AsyncToken>> GeometryBufferPageCopyTokens;
    private:
        //Processes the dwelling import results (the intermediary product of the importing process).
        //It processes the geometry data kept in the import results and uploads them on the buffers to be used by the (graphics) device.
        //Also creates geometry entries which keep the meta data of the uploaded geometries. 
        //These entries then are used in the scene-Model Instance linking process.
        //Which also means that scenes are closely tied with their respective geometry managers and cannot be switched past their constructions.
        //Although it's not expected to have multiple mesh managers around, it's possible.
        //This function is already called internally within the scenes so there is no need to call it explicitly. 
        void UpdateGeometryEntries();

        BufferPageAllocationInfo AllocateFromGeometryBuffers(
            size_t VertexSize,
            size_t IndexSize
        );

        //std::array<std::vector<RENDERER_CORE::BufferAllocator>, MAX_FRAMES_IN_FLIGHT> GeometryBufferPages;
        std::vector<RENDERER_CORE::BufferAllocator> GeometryBufferPages;

        std::array<std::vector<size_t>, MAX_FRAMES_IN_FLIGHT> EraseList;

        //Storage unit that links models with the mesh IDs
        std::unordered_map<std::string, std::vector<size_t>> ModelMeshLinks;

        //Import targets to store imported geometry. CPU side data is to be erased so cleanses per import
        std::vector<MeshImportResult> MeshImportResults;

        //Main storage unit for geometry allocation information
        std::unordered_map<size_t, GeometryEntry> GeometryEntries;
        //std::array<std::unordered_map<size_t, GeometryEntry>,MAX_FRAMES_IN_FLIGHT> GeometryEntries;
        TextureManager* ImportManager = nullptr;
        RENDERER::RendererContext* RendererContext = nullptr;
        RENDERER::ResourceManager* ResourceManagerPtr = nullptr;
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