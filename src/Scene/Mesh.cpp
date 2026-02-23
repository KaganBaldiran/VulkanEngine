#include "Mesh.hpp"
#include "Texture.hpp"

#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>
#include <queue>

#include "../Renderer/MaterialManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void ProcessMeshMaterial(std::string MeshDirectory, RENDERER::TextureManager& ImportManager, aiMaterial* SourceMaterial, SCENE::Material& DestinationMaterial);

VkVertexInputBindingDescription SCENE::Vertex2D::GetBindingDescription()
{
    VkVertexInputBindingDescription BindingDescription;
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(Vertex2D);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return BindingDescription;
}

std::vector<VkVertexInputAttributeDescription> SCENE::Vertex2D::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions(3);
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[0].offset = offsetof(Vertex2D, Position);

    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[1].offset = offsetof(Vertex2D, Color);

    AttributeDescriptions[2].binding = 0;
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[2].offset = offsetof(Vertex2D, UV);

    return AttributeDescriptions;
}

VkVertexInputBindingDescription SCENE::Vertex3D::GetBindingDescription()
{
    VkVertexInputBindingDescription BindingDescription;
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(Vertex3D);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return BindingDescription;
}

std::vector<VkVertexInputAttributeDescription> SCENE::Vertex3D::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions(5);
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[0].offset = offsetof(Vertex3D, Position);

    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[1].offset = offsetof(Vertex3D, UV);

    AttributeDescriptions[2].binding = 0;
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[2].offset = offsetof(Vertex3D, Normal);

    AttributeDescriptions[3].binding = 0;
    AttributeDescriptions[3].location = 3;
    AttributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[3].offset = offsetof(Vertex3D, Tangent);

    AttributeDescriptions[4].binding = 0;
    AttributeDescriptions[4].location = 4;
    AttributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[4].offset = offsetof(Vertex3D, Bitangent);
    return AttributeDescriptions;
}

std::vector<SCENE::GeometryData> SCENE::Import3DGeometry(const char* FilePath, RENDERER::TextureManager& ImportManager)
{
    Assimp::Importer Importer;
    const aiScene* scene = Importer.ReadFile(FilePath,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_PreTransformVertices |
        aiProcess_GenSmoothNormals);
    aiScene* Scene = const_cast<aiScene*>(scene);

    if (nullptr == Scene) {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Failed importing model with the error code [" + std::string(Importer.GetErrorString()) + "].");
        std::cout << "Error code :: " << Importer.GetErrorString() << std::endl;
        throw std::runtime_error("Unable to import a 3D model(" + std::string(FilePath) + ")");
    }

    if (!Scene->HasMeshes()) return std::vector<SCENE::GeometryData>();
    std::string MeshDirectory = std::string(FilePath);
    MeshDirectory = MeshDirectory.substr(0, MeshDirectory.find_last_of("\\"));

    std::queue<aiNode*> NodesToProcess;
    NodesToProcess.push(Scene->mRootNode);
    aiNode* Node = nullptr;

    std::vector<SCENE::GeometryData> GeometryDatas;
    while (!NodesToProcess.empty())
    {
        Node = NodesToProcess.front();
        NodesToProcess.pop();
        for (size_t MeshIndex = 0; MeshIndex < Node->mNumMeshes; MeshIndex++)
        {
            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();

            SCENE::GeometryData NewMesh;
            auto& aiMesh = Scene->mMeshes[Node->mMeshes[MeshIndex]];
            NewMesh.Vertices.reserve(aiMesh->mNumVertices);
            for (size_t VertexIndex = 0; VertexIndex < aiMesh->mNumVertices; VertexIndex++)
            {
                Vertex3D Vertex;
                auto& AiVertexPosition = aiMesh->mVertices[VertexIndex];
                Vertex.Position = { AiVertexPosition.x , AiVertexPosition.y , AiVertexPosition.z };

                maxX = std::max(Vertex.Position.x, maxX);
                maxY = std::max(Vertex.Position.y, maxY);
                maxZ = std::max(Vertex.Position.z, maxZ);
                minX = std::min(Vertex.Position.x, minX);
                minY = std::min(Vertex.Position.y, minY);
                minZ = std::min(Vertex.Position.z, minZ);

                if (aiMesh->HasNormals())
                {
                    auto& AiVertexNormal = aiMesh->mNormals[VertexIndex];
                    Vertex.Normal = { AiVertexNormal.x , AiVertexNormal.y , AiVertexNormal.z };
                }

                if (aiMesh->HasTangentsAndBitangents())
                {
                    auto& AiVertexTangent = aiMesh->mTangents[VertexIndex];
                    auto& AiVertexBitangent = aiMesh->mBitangents[VertexIndex];
                    Vertex.Tangent = { AiVertexTangent.x , AiVertexTangent.y , AiVertexTangent.z };
                    Vertex.Bitangent = { AiVertexBitangent.x , AiVertexBitangent.y , AiVertexBitangent.z };
                }

                if (aiMesh->HasTextureCoords(0))
                {
                    auto& AiVertexTextCoords = aiMesh->mTextureCoords[0][VertexIndex];
                    Vertex.UV = { AiVertexTextCoords.x , AiVertexTextCoords.y };
                }
                NewMesh.Vertices.push_back(Vertex);
            }

            NewMesh.Indices.reserve(aiMesh->mNumFaces * 3);
            for (size_t FaceIndex = 0; FaceIndex < aiMesh->mNumFaces; FaceIndex++)
            {
                auto& Face = aiMesh->mFaces[FaceIndex];
                for (size_t Index = 0; Index < Face.mNumIndices; Index++)
                {
                    NewMesh.Indices.push_back(Face.mIndices[Index]);
                }
            }

            aiMaterial* material = scene->mMaterials[aiMesh->mMaterialIndex];
            ProcessMeshMaterial(MeshDirectory, ImportManager, material, NewMesh.MeshMaterial);

            NewMesh.BoundingBox.Center = glm::vec4((glm::vec3(maxX, maxY, maxZ) + glm::vec3(minX, minY, minZ)) * 0.5f,0.0f);
            NewMesh.BoundingBox.Extends = glm::vec4((glm::vec3(maxX, maxY, maxZ) - glm::vec3(minX, minY, minZ)) * 0.5f,0.0f);
            GeometryDatas.push_back(NewMesh);
        }

        for (size_t i = 0; i < Node->mNumChildren; i++)
        {
            NodesToProcess.push(*(Node->mChildren + i));
        }
    }
    return GeometryDatas;
}


void LoadMaterialTextures(std::string MeshDirectory, RENDERER::TextureManager& ImportManager, aiMaterial* SourceMaterial, aiTextureType Type, uint64_t& MaterialTextureReferenceIndex)
{
    auto& ImportRegistries = ImportManager.ImportRegistries;
    auto& ImportQueue = ImportManager.ImportQueue;
    for (size_t i = 0; i < SourceMaterial->GetTextureCount(Type); i++)
    {
        aiString str;
        SourceMaterial->GetTexture(Type, i, &str);
        bool skip = false;
        auto Iterator = ImportRegistries.find(str.C_Str());
        if (Iterator != ImportRegistries.end())
        {
            MaterialTextureReferenceIndex = Iterator->second;
            skip = true;
            //break;
        }
        if (!skip)
        {
            SCENE::Texture NewTexture;
            MaterialTextureReferenceIndex = NewTexture.GetHandleID();
            ImportQueue.push({ MeshDirectory + "\\" + std::string(str.C_Str()),NewTexture.GetHandleID() });
        }
    }
}

void ProcessMeshMaterial(std::string MeshDirectory, RENDERER::TextureManager& ImportManager,aiMaterial* SourceMaterial,SCENE::Material &DestinationMaterial)
{
    aiColor4D color;
    float value;
    aiString Path;
    if (aiReturn_SUCCESS == SourceMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        DestinationMaterial.Albedo.x = color.r;
        DestinationMaterial.Albedo.y = color.g;
        DestinationMaterial.Albedo.z = color.b;
        DestinationMaterial.Albedo.w = color.a;
    }
    if (aiReturn_SUCCESS == SourceMaterial->Get(AI_MATKEY_METALLIC_FACTOR, value)) {
        DestinationMaterial.Metallic = value;
    }
    if (aiReturn_SUCCESS == SourceMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, value)) {
        DestinationMaterial.Roughness = value;
    }
    if (aiReturn_SUCCESS == SourceMaterial->Get(AI_MATKEY_OPACITY, value)) {
        DestinationMaterial.Opacity = value;
    }

    LoadMaterialTextures(MeshDirectory,ImportManager, SourceMaterial, aiTextureType_DIFFUSE, DestinationMaterial.ReferencedTextures[static_cast<size_t>(SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO)]);
    if (SourceMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path) == AI_SUCCESS) {
        LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_DIFFUSE_ROUGHNESS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(SCENE::MATERIAL_TEXTURE_TYPE_ROUGHNESS)]);
    }
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_METALNESS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(SCENE::MATERIAL_TEXTURE_TYPE_METALLIC)]);
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_OPACITY, DestinationMaterial.ReferencedTextures[static_cast<size_t>(SCENE::MATERIAL_TEXTURE_TYPE_OPACITY)]);
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_NORMALS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(SCENE::MATERIAL_TEXTURE_TYPE_NORMAL_MAP)]);
}

void SCENE::Transformation::Translate(glm::vec3 TranslationVector)
{
    TranslationMatrix = glm::translate(TranslationMatrix, TranslationVector);
}

void SCENE::Transformation::Rotate(float RotationInDegrees, glm::vec3 Axis)
{
    RotationMatrix = glm::rotate(RotationMatrix, glm::radians(RotationInDegrees), Axis);
}

void SCENE::Transformation::Scale(glm::vec3 ScaleCoefficient)
{
    ScalingMatrix = glm::scale(ScalingMatrix, ScaleCoefficient);
}

glm::vec3 SCENE::Transformation::GetPosition()
{
    return { TranslationMatrix[0][3],TranslationMatrix[1][3],TranslationMatrix[2][3] };
}

void SCENE::Transformation::SetTranslationMatrix(const glm::mat4& Matrix)
{
    TranslationMatrix = Matrix;
}

void SCENE::Transformation::SetRotationMatrix(const glm::mat4& Matrix)
{
    RotationMatrix = Matrix;
}

void SCENE::Transformation::SetScalingMatrix(const glm::mat4& Matrix)
{
    ScalingMatrix = Matrix;
}
