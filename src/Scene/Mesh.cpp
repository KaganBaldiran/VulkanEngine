#include "Mesh.hpp"
#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h>

#include <queue>
#include "MaterialManager.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void ProcessMeshMaterial(std::string MeshDirectory,VKSCENE::TextureImportManager& ImportManager, aiMaterial* SourceMaterial, VKSCENE::Material& DestinationMaterial);

VKSCENE::MeshImporter::MeshImporter(TextureImportManager& ImportManager)
{
    Create(ImportManager);
}

void VKSCENE::MeshImporter::Create(TextureImportManager& ImportManager)
{
    this->ImportManager = &ImportManager;
}

void VKSCENE::MeshImporter::AppendImportTask(ModelImportInfo ImportInfo)
{
    ImportQueue.push(ImportInfo);
}

void VKSCENE::MeshImporter::SubmitImport()
{
    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto Import = std::move(ImportQueue.front());
        Futures.push_back(std::async(std::launch::async, VKSCENE::Import3Dmodel, Import.ModelFilePath, std::ref(*Import.DestinationModel), std::ref(*ImportManager)));
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_INFO, "Imported model [" + std::string(Import.ModelFilePath) + "].");
        ImportQueue.pop();
    }
}

void VKSCENE::MeshImporter::WaitImportIdle()
{
    for (auto& future : Futures)
    {
        future.get();
    }
    Futures.clear();

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Models were imported in: " << DeltaTime << " seconds" << std::endl;
    LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_INFO, "Models were imported in " + std::to_string(DeltaTime) + " seconds.");
}

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

void VKSCENE::Import3Dmodel(const char* FilePath, Model3D& DstModel,VKSCENE::TextureImportManager &ImportManager)
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
    std::string MeshDirectory = std::string(FilePath);
    MeshDirectory = MeshDirectory.substr(0,MeshDirectory.find_last_of("\\"));

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
            ProcessMeshMaterial(MeshDirectory,ImportManager, material, NewMesh.MeshMaterial);
            DstModel.Meshes.push_back(NewMesh);
        }

        for (size_t i = 0; i < Node->mNumChildren; i++)
        {
            NodesToProcess.push(*(Node->mChildren + i));
        }
    }
}

VKSCENE::BatchInfo VKSCENE::BatchModels(std::vector<VKSCENE::Model3D>& Models,std::vector<DrawInfo> &DestinationDrawInfos)
{
    VKSCENE::BatchInfo Result{};
    uint32_t IndexOffset = 0;
    for (auto& Model : Models)
    {
        for (auto& Mesh : Model.Meshes)
        {
            if (!Mesh.Enabled) continue;
            VKSCENE::DrawInfo Info;

            Info.VertexOffset = Result.Vertices.size();
            Result.Vertices.insert(Result.Vertices.end(), Mesh.Vertices.begin(), Mesh.Vertices.end());
           
            Info.IndexCount = Mesh.Indices.size(); 
            Info.FirstIndex = IndexOffset;

            Result.Indices.insert(Result.Indices.end(), Mesh.Indices.begin(), Mesh.Indices.end());
            
            IndexOffset += Mesh.Indices.size();
            DestinationDrawInfos.push_back(Info);
        }
    }
    return Result;
}

void LoadMaterialTextures(std::string MeshDirectory,VKSCENE::TextureImportManager& ImportManager, aiMaterial* SourceMaterial, aiTextureType Type, uint64_t& MaterialTextureReferenceIndex)
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
            VKSCENE::Texture NewTexture;
            MaterialTextureReferenceIndex = NewTexture.ResourceID;
            ImportQueue.push({ MeshDirectory + "\\" + std::string(str.C_Str()),NewTexture.ResourceID});
        }
    }
}

void ProcessMeshMaterial(std::string MeshDirectory, VKSCENE::TextureImportManager& ImportManager,aiMaterial* SourceMaterial,VKSCENE::Material &DestinationMaterial)
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

    LoadMaterialTextures(MeshDirectory,ImportManager, SourceMaterial, aiTextureType_DIFFUSE, DestinationMaterial.ReferencedTextures[static_cast<size_t>(VKSCENE::MATERIAL_TEXTURE_TYPE_ALBEDO)]);
    if (SourceMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path) == AI_SUCCESS) {
        LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_DIFFUSE_ROUGHNESS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(VKSCENE::MATERIAL_TEXTURE_TYPE_ROUGHNESS)]);
    }
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_METALNESS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(VKSCENE::MATERIAL_TEXTURE_TYPE_METALLIC)]);
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_OPACITY, DestinationMaterial.ReferencedTextures[static_cast<size_t>(VKSCENE::MATERIAL_TEXTURE_TYPE_OPACITY)]);
    LoadMaterialTextures(MeshDirectory, ImportManager, SourceMaterial, aiTextureType_NORMALS, DestinationMaterial.ReferencedTextures[static_cast<size_t>(VKSCENE::MATERIAL_TEXTURE_TYPE_NORMAL_MAP)]);
}

VKSCENE::BatchInfo VKSCENE::BatchModels(std::vector<VKSCENE::Model3D*> Models, std::vector<DrawInfo>& DestinationDrawInfos)
{
    VKSCENE::BatchInfo Result{};
    uint32_t IndexOffset = 0;
    for (auto& Model : Models)
    {
        for (auto& Mesh : Model->Meshes)
        {
            if (!Mesh.Enabled) continue;
            VKSCENE::DrawInfo Info;

            Info.VertexOffset = Result.Vertices.size();
            Result.Vertices.insert(Result.Vertices.end(), Mesh.Vertices.begin(), Mesh.Vertices.end());

            Info.IndexCount = Mesh.Indices.size();
            Info.FirstIndex = IndexOffset;

            Result.Indices.insert(Result.Indices.end(), Mesh.Indices.begin(), Mesh.Indices.end());

            IndexOffset += Mesh.Indices.size();
            DestinationDrawInfos.push_back(Info);
        }
    }
    return Result;
}

void VKSCENE::Model3D::SetModelMeshesUpdateMode(MeshUpdateMode MeshUpdateMode)
{
    for (auto& mesh : this->Meshes)
    {
        mesh.meshUpdateMode = MeshUpdateMode;
    }
}
