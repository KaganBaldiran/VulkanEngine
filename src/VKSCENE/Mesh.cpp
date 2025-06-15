#include "Mesh.hpp"
#include <queue>
#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>

VkVertexInputBindingDescription VKSCENE::Vertex2D::GetBindingDescription()
{
    VkVertexInputBindingDescription BindingDescription;
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(Vertex2D);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return BindingDescription;
}

std::vector<VkVertexInputAttributeDescription> VKSCENE::Vertex2D::GetAttributeDescriptions()
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

VkVertexInputBindingDescription VKSCENE::Vertex3D::GetBindingDescription()
{
    VkVertexInputBindingDescription BindingDescription;
    BindingDescription.binding = 0;
    BindingDescription.stride = sizeof(Vertex3D);
    BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return BindingDescription;
}

std::vector<VkVertexInputAttributeDescription> VKSCENE::Vertex3D::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions(4);
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[0].offset = offsetof(Vertex3D, Position);

    AttributeDescriptions[1].binding = 0;
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[1].offset = offsetof(Vertex3D, Color);

    AttributeDescriptions[2].binding = 0;
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    AttributeDescriptions[2].offset = offsetof(Vertex3D, UV);

    AttributeDescriptions[3].binding = 0;
    AttributeDescriptions[3].location = 3;
    AttributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    AttributeDescriptions[3].offset = offsetof(Vertex3D, Normal);
    return AttributeDescriptions;
}

void VKSCENE::Import3Dmodel(const char* FilePath, Model3D& DstModel)
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
        std::cout << "Error code :: " << Importer.GetErrorString() << std::endl;
        throw std::runtime_error("Unable to import a 3D model(" + std::string(FilePath) + ")");
    }

    if (!Scene->HasMeshes()) return;

    std::queue<aiNode*> NodesToProcess;
    NodesToProcess.push(Scene->mRootNode);
    aiNode* Node = nullptr;
    while (!NodesToProcess.empty())
    {
        Node = NodesToProcess.front();
        NodesToProcess.pop();
        for (size_t MeshIndex = 0; MeshIndex < Node->mNumMeshes; MeshIndex++)
        {
            Mesh NewMesh;
            auto& aiMesh = Scene->mMeshes[Node->mMeshes[MeshIndex]];
            NewMesh.Vertices.reserve(aiMesh->mNumVertices);
            for (size_t VertexIndex = 0; VertexIndex < aiMesh->mNumVertices; VertexIndex++)
            {
                Vertex3D Vertex;
                auto& AiVertexPosition = aiMesh->mVertices[VertexIndex];
                Vertex.Position = { AiVertexPosition.x , AiVertexPosition.y , AiVertexPosition.z };

                if (aiMesh->HasNormals())
                {
                    auto& AiVertexNormal = aiMesh->mNormals[VertexIndex];
                    Vertex.Normal = { AiVertexNormal.x , AiVertexNormal.y , AiVertexNormal.z };
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
            DstModel.Meshes.push_back(NewMesh);
        }

        for (size_t i = 0; i < Node->mNumChildren; i++)
        {
            NodesToProcess.push(*(Node->mChildren + i));
        }
    }
}

VKSCENE::BatchInfo VKSCENE::BatchModels(std::vector<VKSCENE::Model3D>& Models)
{
    VKSCENE::BatchInfo Result{};
    uint32_t IndexOffset = 0;
    for (auto& Model : Models)
    {
        for (auto& Mesh : Model.Meshes)
        {
            if (!Mesh.Enabled) continue;

            Mesh.Info.VertexOffset = Result.Vertices.size();
            Result.Vertices.insert(Result.Vertices.end(), Mesh.Vertices.begin(), Mesh.Vertices.end());
           
            Mesh.Info.IndexCount = Mesh.Indices.size(); 
            Mesh.Info.FirstIndex = IndexOffset;

            Result.Indices.insert(Result.Indices.end(), Mesh.Indices.begin(), Mesh.Indices.end());
            
            IndexOffset += Mesh.Indices.size();
        }
    }
    return Result;
}
