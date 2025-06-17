#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../VKCORE/VulkanUtils.hpp" 

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace VKSCENE
{
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

    struct Mesh
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
        DrawInfo Info;
        bool Enabled = true;
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
    };

    struct BatchInfo
    {
        std::vector<Vertex3D> Vertices;
        std::vector<uint32_t> Indices;
    };

    void Import3Dmodel(const char* FilePath, Model3D& DstModel);
    BatchInfo BatchModels(std::vector<Model3D> &Models);
    BatchInfo BatchModels(std::vector<Model3D*> Models);
}
