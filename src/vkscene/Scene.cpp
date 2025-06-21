#include "Scene.hpp"
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../app/RendererContext.hpp"

void VKSCENE::MeshImporter::AppendImportTask(ModelImportInfo ImportInfo)
{
    ImportQueue.push(ImportInfo);
}

void VKSCENE::MeshImporter::SubmitImport()
{
    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto& Import = std::move(ImportQueue.front());
        Futures.push_back(std::async(std::launch::async, VKSCENE::Import3Dmodel, Import.ModelFilePath, std::ref(*Import.DestinationModel)));
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
}

VKSCENE::Scene::Scene(VKAPP::RendererContext& RendererContext)
{
    Create(RendererContext);
}

void VKSCENE::Scene::Create(VKAPP::RendererContext& RendererContext)
{
    //Light SSBO descriptor set
    DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2 * MAX_FRAMES_IN_FLIGHT} },
        MAX_FRAMES_IN_FLIGHT, RendererContext.DeviceContext.logicalDevice
    );

    DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, DescriptorPool.descriptorPool, RendererContext.SceneDescriptorSetLayouts, DescriptorSets);
}

void VKSCENE::Scene::Destroy(VKAPP::RendererContext& RendererContext)
{
    DescriptorPool.Destroy(RendererContext.DeviceContext.logicalDevice);
    DestroyMeshBuffers(RendererContext);
    DestroyLightBuffers(RendererContext);
}

void VKSCENE::Scene::UpdateDynamicLightBuffers()
{
    VkDeviceSize DynamicBufferSize = sizeof(LightData) * DynamicLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[0].MappedMemory);

    bool IsAnyUpdated = false;
    for (size_t i = 0; i < DynamicLights.size(); i++)
    {
        if (DynamicLights[i]->Updated)
        {
            Destination[i] = DynamicLights[i]->Data;
            DynamicLights[i]->Updated = false;
            IsAnyUpdated = true;
        }
    }
    if (IsAnyUpdated)
    {
        for (size_t i = 1; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            memcpy(DynamicLightSSBO[i].MappedMemory, Destination, DynamicBufferSize);
        }
    }
}

void VKSCENE::Scene::UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame)
{
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[CurrentFrame].MappedMemory);
    for (size_t i = 0; i < DynamicLights.size(); i++)
    {
        if (DynamicLights[i]->Updated)
        {
            Destination[i] = DynamicLights[i]->Data;
            DynamicLights[i]->Updated = false;
        }
    }
}

void VKSCENE::Scene::UpdateStaticLightBuffers(VKAPP::RendererContext& RendererContext)
{
    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    for (size_t i = 0; i < StaticLights.size(); i++)
    {
        if (StaticLights[i]->Updated)
        {
            Destination[i] = StaticLights[i]->Data;
            StaticLights[i]->Updated = false;
        }
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CopyBuffer(
            StaticLightStagingBuffer.Buffer.BufferObject,
            StaticLightSSBO[i].BufferObject,
            StaticBufferSize,
            RendererContext.DeviceContext.logicalDevice,
            RendererContext.CommandPool.commandPool,
            RendererContext.DeviceContext.GraphicsQueue
        );
    }
}

void VKSCENE::Scene::UpdateStaticFrameLightBuffers(VKAPP::RendererContext& RendererContext, uint32_t CurrentFrame)
{
    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    for (size_t i = 0; i < StaticLights.size(); i++)
    {
        if (StaticLights[i]->Updated)
        {
            Destination[i] = StaticLights[i]->Data;
            StaticLights[i]->Updated = false;
        }
    }
    VKCORE::CopyBuffer(
        StaticLightStagingBuffer.Buffer.BufferObject,
        StaticLightSSBO[CurrentFrame].BufferObject,
        StaticBufferSize,
        RendererContext.DeviceContext.logicalDevice,
        RendererContext.CommandPool.commandPool,
        RendererContext.DeviceContext.GraphicsQueue
    );
}

void VKSCENE::Scene::CreateMeshBuffers(VKAPP::RendererContext& RendererContext)
{
    if (Entities.empty()) return;
    std::vector<VKSCENE::Model3D*> Models(Entities.size());
    for (size_t i = 0; i < Entities.size(); i++)
    {
        Models[i] = &Entities[i]->Model;
    }

    VKSCENE::BatchInfo ModelBatch = VKSCENE::BatchModels(Models);
    VkDeviceSize VertexBufferSize = ModelBatch.Vertices.size() * sizeof(VKSCENE::Vertex3D);
    VkDeviceSize IndexBufferSize = ModelBatch.Indices.size() * sizeof(uint32_t);

    VKCORE::UploadDataToDeviceLocalBuffer(
        RendererContext.DeviceContext.logicalDevice,
        RendererContext.DeviceContext.physicalDevice,
        RendererContext.CommandPool.commandPool,
        RendererContext.DeviceContext.GraphicsQueue,
        ModelBatch.Vertices.data(),
        VertexBufferSize,
        SceneVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    VKCORE::UploadDataToDeviceLocalBuffer(
        RendererContext.DeviceContext.logicalDevice,
        RendererContext.DeviceContext.physicalDevice,
        RendererContext.CommandPool.commandPool,
        RendererContext.DeviceContext.GraphicsQueue,
        ModelBatch.Indices.data(),
        IndexBufferSize,
        SceneIndexBuffer,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
}

void VKSCENE::Scene::UpdateMeshBuffers(VKAPP::RendererContext& RendererContext)
{
    DestroyMeshBuffers(RendererContext);
    CreateMeshBuffers(RendererContext);
}

void VKSCENE::Scene::CreateLightBuffers(VKAPP::RendererContext& RendererContext, uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount)
{
    VkDeviceSize DynamicLightBufferSize = sizeof(LightData) * MaxDynamicLightCount;
    VkDeviceSize StaticLightBufferSize = sizeof(LightData) * MaxStaticLightCount;

    DynamicLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    StaticLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(
            RendererContext.DeviceContext.physicalDevice,
            RendererContext.DeviceContext.logicalDevice,
            DynamicLightBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            DynamicLightSSBO[i].Buffer
        );
        DynamicLightSSBO[i].Map(RendererContext.DeviceContext.logicalDevice, 0, DynamicLightBufferSize, 0);

        VKCORE::CreateBuffer(
            RendererContext.DeviceContext.physicalDevice,
            RendererContext.DeviceContext.logicalDevice,
            StaticLightBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            StaticLightSSBO[i]
        );
    }

    VKCORE::CreateBuffer(
        RendererContext.DeviceContext.physicalDevice,
        RendererContext.DeviceContext.logicalDevice,
        StaticLightBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        StaticLightStagingBuffer.Buffer
    );
    StaticLightStagingBuffer.Map(RendererContext.DeviceContext.logicalDevice, 0, StaticLightBufferSize, 0);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteBuffer StaticSSBOwrite(StaticLightSSBO[i], StaticLightBufferSize, 0, DescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::DescriptorSetWriteBuffer DynamicSSBOwrite(DynamicLightSSBO[i].Buffer, DynamicLightBufferSize, 1, DescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::WriteDescriptorSets(RendererContext.DeviceContext.logicalDevice, { StaticSSBOwrite,DynamicSSBOwrite }, {});
    }
}

void VKSCENE::Scene::DestroyMeshBuffers(VKAPP::RendererContext& RendererContext)
{
     SceneVertexBuffer.Destroy(RendererContext.DeviceContext.logicalDevice);
     SceneIndexBuffer.Destroy(RendererContext.DeviceContext.logicalDevice);
}

void VKSCENE::Scene::DestroyLightBuffers(VKAPP::RendererContext& RendererContext)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        StaticLightSSBO[i].Destroy(RendererContext.DeviceContext.logicalDevice);
        DynamicLightSSBO[i].Buffer.Destroy(RendererContext.DeviceContext.logicalDevice);
    }
    StaticLightStagingBuffer.Buffer.Destroy(RendererContext.DeviceContext.logicalDevice);
}

