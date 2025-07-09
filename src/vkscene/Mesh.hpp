#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../VKCORE/VulkanUtils.hpp" 

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <future>
#include <queue>

#include "Material.hpp"

namespace VKSCENE
{
    class TextureImportManager;

    struct Vertex2D {
        glm::vec2 Position;
        glm::vec3 Color;
        glm::vec2 UV;

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
    };

    struct Vertex3D {
        glm::vec3 Position;
        glm::vec3 Color;
        glm::vec2 UV;
        glm::vec3 Normal;

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
    };

    struct DrawInfo
    {
        uint32_t IndexCount;
        uint32_t FirstIndex;
        uint32_t VertexOffset;
    };

    enum MeshUpdateMode
    {
        /// <summary>
        ///Keeps a buffer for each frame in flight.
        ///Updates the buffers separately for each frame.
        ///Requires high memory usage.
        /// Suitable for dynamic meshes that get updated very often.
        /// </summary>
        MESH_UPDATE_MODE_PERFORMANCE = 0,
        /// <summary>
        /// On update simply creates a second buffer and deletes the older one once all the frames that use it are rendered.
        /// Suitable for rarely updated large meshes.
        /// </summary>
        MESH_UPDATE_MODE_BALANCED = 1,
        /// <summary>
        /// Waits idle for the GPU to finish rendering existing frames before placing the update on the existing buffer.
        /// Lowest memory consumption and performance.
        /// Suitable for static meshes that get updated only on scene loading.
        /// </summary>
        MESH_UPDATE_MODE_MEMORY_SAVING = 2
    };

    struct Mesh
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
        Material MeshMaterial;
        DrawInfo Info;
        bool Enabled = true;
        MeshUpdateMode meshUpdateMode = MESH_UPDATE_MODE_PERFORMANCE;
    };

    struct Transformation
    {
        glm::mat4 TranslationMatrix = glm::mat4(1.0f);
        glm::mat4 ScalingMatrix = glm::mat4(1.0f);
        glm::mat4 RotationMatrix = glm::mat4(1.0f);

        glm::mat4 GetModelMatrix() { return TranslationMatrix * RotationMatrix * ScalingMatrix; };
    };

    struct Model3D
    {
        std::vector<Mesh> Meshes;
        Transformation transformation;

        /// <summary>
        /// Sets the update mode for all of the model meshes.
        /// </summary>
        void SetModelMeshesUpdateMode(MeshUpdateMode MeshUpdateMode);
    };

    struct ModelImportInfo
    {
        VKSCENE::Model3D* DestinationModel;
        const char* ModelFilePath;
    };

    class MeshImporter
    {
    public:
        MeshImporter(TextureImportManager& ImportManager);
        MeshImporter() = default;
        void Create(TextureImportManager& ImportManager);

        void AppendImportTask(ModelImportInfo ImportInfo);
        void SubmitImport();
        void WaitImportIdle();
        std::vector<std::future<void>> Futures;
        std::queue<ModelImportInfo> ImportQueue;
        double StartingTime;
    private:
        TextureImportManager *ImportManager = nullptr;
    };

    struct BatchInfo
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
    };

    void Import3Dmodel(const char* FilePath, Model3D& DstModel,VKSCENE::TextureImportManager& ImportManager);
    BatchInfo BatchModels(std::vector<Model3D> &Models);
    BatchInfo BatchModels(std::vector<Model3D*> Models);
}
