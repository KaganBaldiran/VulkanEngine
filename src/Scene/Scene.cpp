#include "Scene.hpp"
#include "Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../Renderer/RendererContext.hpp"
#include "Cubemap.hpp"
#include "../Renderer/Core/VulkanPipeline.hpp"

#include "DependencyManager.hpp"
#include "MaterialManager.hpp"

VKSCENE::Scene::Scene(VKAPP::RendererContext& RendererContext,TextureImportManager &Manager)
{
    Create(RendererContext, Manager);
}

void VKSCENE::Scene::Create(VKAPP::RendererContext& RendererContext,TextureImportManager& Manager)
{
    this->MeshBuffers.Create(RendererContext);
    MeshBuffers.Owner = this;
    TextureManager = &Manager;
   
    //Light SSBO descriptor set
    SceneDescriptorPool.Create(
        { 
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * MAX_FRAMES_IN_FLIGHT}
        },
        5 * MAX_FRAMES_IN_FLIGHT, 
        RendererContext.DeviceContext.logicalDevice
    );
    
    SceneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.SceneDescriptorSetLayouts, SceneDescriptorSets);
    
    IndirectDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VKCORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.IndirectDescriptorSetLayouts, IndirectDescriptorSets);
    
    TexturesIndexBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    TexturesIndexStagingBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    this->RendererContext = &RendererContext;
    DrawCubeMap = true;
}

void VKSCENE::Scene::Destroy()
{
    SceneDescriptorPool.Destroy(RendererContext->DeviceContext.logicalDevice);
    DestroyMeshBuffers();
    DestroyMeshTextureDescriptors();
    DestroyLightBuffers();
}

void VKSCENE::Scene::LinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesAppendList[i].push_back(&Instance);
    }
}

void VKSCENE::Scene::LinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesAppendList[i].insert(ModelInstancesAppendList[i].end(), Instances.begin(), Instances.end());
    }
}

void VKSCENE::Scene::UnlinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesEraseList[i].push_back(&Instance);
    }
}

void VKSCENE::Scene::UnlinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesEraseList[i].insert(ModelInstancesEraseList[i].end(), Instances.begin(), Instances.end());
    }
}

void VKSCENE::Scene::SetCubemap(Cubemap& DestinationCubeMap)
{
    SceneCubeMap = &DestinationCubeMap;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteImage CubemapTextureWrite(SceneCubeMap->ConvolutionSampleImageView, SceneCubeMap->ConvolutionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, {}, { CubemapTextureWrite });
    }
}

void VKSCENE::Scene::SetCamera(Camera3D& Camera)
{
    this->Camera = &Camera;
}

void VKSCENE::Scene::UpdateDynamicLightBuffers()
{
    if (!this->DependencyManager) return;

    VkDeviceSize DynamicBufferSize = sizeof(LightData) * DynamicLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[0].MappedMemory);

    bool IsAnyUpdated = false;
    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
            IsAnyUpdated = true;
        }
        i++;
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
    if (!this->DependencyManager) return;

    LightData* Destination = reinterpret_cast<LightData*>(DynamicLightSSBO[CurrentFrame].MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : DynamicLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
        {
            Destination[i] = Light->Data;
        }
        i++;
    }
}

void VKSCENE::Scene::UpdateStaticLightBuffers()
{
    if (!this->DependencyManager) return;

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
        }
        i++;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CopyBuffer(
            StaticLightStagingBuffer.Buffer.BufferObject,
            StaticLightSSBO[i].BufferObject,
            StaticBufferSize,
            RendererContext->DeviceContext.logicalDevice,
            RendererContext->CommandPool.commandPool,
            RendererContext->DeviceContext.GraphicsQueue
        );
    }
}

void VKSCENE::Scene::UpdateStaticFrameLightBuffers(uint32_t CurrentFrame)
{
    if (!this->DependencyManager) return;

    VkDeviceSize StaticBufferSize = sizeof(LightData) * StaticLights.size();
    LightData* Destination = reinterpret_cast<LightData*>(StaticLightStagingBuffer.MappedMemory);
    size_t i = 0;
    for (const auto& [id, Light] : StaticLights)
    {
        if (DependencyManager->IsResourceDirty(Light->ResourceID))
        {
            Destination[i] = Light->Data;
            Light->Updated = false;
        }
        i++;
    }
    VKCORE::CopyBuffer(
        StaticLightStagingBuffer.Buffer.BufferObject,
        StaticLightSSBO[CurrentFrame].BufferObject,
        StaticBufferSize,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );
}


void VKSCENE::Scene::CreateLightBuffers(uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount)
{
    VkDeviceSize DynamicLightBufferSize = sizeof(LightData) * MaxDynamicLightCount;
    VkDeviceSize StaticLightBufferSize = sizeof(LightData) * MaxStaticLightCount;

    DynamicLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    StaticLightSSBO.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            DynamicLightBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            DynamicLightSSBO[i].Buffer
        );
        DynamicLightSSBO[i].Map(RendererContext->DeviceContext.logicalDevice, 0, DynamicLightBufferSize, 0);

        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            StaticLightBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            StaticLightSSBO[i]
        );
    }

    VKCORE::CreateBuffer(
        RendererContext->DeviceContext.physicalDevice,
        RendererContext->DeviceContext.logicalDevice,
        StaticLightBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        StaticLightStagingBuffer.Buffer
    );
    StaticLightStagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StaticLightBufferSize, 0);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::DescriptorSetWriteBuffer StaticSSBOwrite(StaticLightSSBO[i], StaticLightBufferSize, 0, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::DescriptorSetWriteBuffer DynamicSSBOwrite(DynamicLightSSBO[i].Buffer, DynamicLightBufferSize, 1, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { StaticSSBOwrite,DynamicSSBOwrite }, {});
    }
}

void VKSCENE::Scene::DestroyMeshBuffers()
{
    MeshBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::Scene::DestroyLightBuffers()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        StaticLightSSBO[i].Destroy(RendererContext->DeviceContext.logicalDevice);
        DynamicLightSSBO[i].Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    }
    StaticLightStagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void VKSCENE::Scene::MarkResourceChanged(SceneResource* Resource)
{
}

void VKSCENE::Scene::FlushPendingUpdates(SceneUpdateType Type, uint32_t FrameIndex)
{
    if (!Type) return;

    bool IsAllFrames = FrameIndex == std::numeric_limits<uint32_t>::max();
    if (!IsAllFrames && FrameIndex >= MAX_FRAMES_IN_FLIGHT) throw std::runtime_error("Given Frame Index is larger than the maximum frames in flight count!");

    //Update all the frames if IsAllFrames
    for (uint32_t i = (IsAllFrames ? 0 : FrameIndex); i < (IsAllFrames ? MAX_FRAMES_IN_FLIGHT : (FrameIndex + 1)); i++)
    {
        if ((Type & SCENE_UPDATE_TYPE_LINK_MESHES) && !ModelInstancesAppendList.empty())
        {
            MeshAppendInfo Info{};
            Info.FrameIndex = i;
            Info.ModelInstances = ModelInstancesAppendList[i];
            Info.TargetDescriptorSets = IndirectDescriptorSets;
            MeshBuffers.AppendModels(Info);

            ModelInstancesAppendList[i].clear();
        }
        if ((Type & SCENE_UPDATE_TYPE_UNLINK_MESHES) && !ModelInstancesEraseList.empty())
        {
            MeshEraseInfo Info{};
            Info.FrameIndex = i;
            Info.ModelInstances = ModelInstancesEraseList[i];
            Info.TargetDescriptorSets = IndirectDescriptorSets;
            MeshBuffers.EraseModels(Info);

            ModelInstancesEraseList[i].clear();
        }
        if (Type & SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS)
        {
            UpdateMeshTransformations(i);
        }
        if (Type & SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS)
        {
            MeshTextureUpdateInfo Info{};
            Info.FrameIndex = i;
            Info.TargetDescriptorSets = MeshTexturesDescriptor.DescriptorSets;
            Info.TextureImportManagerPtr = this->TextureManager;

            MeshBuffers.UpdateTextureDescriptors(Info);
        }
    }
}

void VKSCENE::Scene::UpdateMeshTransformations(uint32_t CurrentFrame)
{
    this->MeshBuffers.UpdateMeshTransformations(CurrentFrame);
}

void VKSCENE::Scene::CreateMeshTextureDescriptors(
    uint32_t MaxTextures
)
{
    uint32_t BlockSize = 250;
    ActualTextureUpperBound = static_cast<uint32_t>(glm::ceil((float)MaxTextures / (float)BlockSize)) * BlockSize;
    std::cout << "Creating texture descriptor set with upper bound: " << ActualTextureUpperBound << "\n";

    MeshTexturesDescriptor.DescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MAX_FRAMES_IN_FLIGHT * ActualTextureUpperBound},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,MAX_FRAMES_IN_FLIGHT * 3} },
        MAX_FRAMES_IN_FLIGHT,
        RendererContext->DeviceContext.logicalDevice,
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
    );

    VkDescriptorBindingFlags LayoutFlags[2] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 
        0 
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo BindingFlags{};
    BindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    BindingFlags.pBindingFlags = LayoutFlags;
    BindingFlags.bindingCount = 2;

    MeshTexturesDescriptor.Layout.AppendLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        ActualTextureUpperBound,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    MeshTexturesDescriptor.Layout.AppendLayoutBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        1,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    MeshTexturesDescriptor.Layout.CreateLayout(
        RendererContext->DeviceContext.logicalDevice,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        &BindingFlags
    );

    VKCORE::AllocateDescriptorSets(
        RendererContext->DeviceContext.logicalDevice,
        MeshTexturesDescriptor.DescriptorSets.size(),
        MeshTexturesDescriptor.DescriptorPool.descriptorPool,
        MeshTexturesDescriptor.Layout.descriptorSetLayout,
        MeshTexturesDescriptor.DescriptorSets.data()
    );

    CurrentGbufferPassPipeline = RendererContext->AppendGbufferPassPipeline(MeshTexturesDescriptor.Layout.descriptorSetLayout, ActualTextureUpperBound);
   
   /* VkDeviceSize TexturesIndexBufferSize = sizeof(int) * ActualTextureUpperBound;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            TexturesIndexBuffers[i]
        );

        VKCORE::CreateBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            TexturesIndexStagingBuffers[i]
        );

        VKCORE::DescriptorSetWriteBuffer TexturesIndexSSBOWrite(
            TexturesIndexBuffers[i],
            TexturesIndexBufferSize,
            1,
            MeshTexturesDescriptor.DescriptorSets[i],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        VKCORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, { TexturesIndexSSBOWrite }, {});
    }*/
}

void VKSCENE::Scene::DestroyMeshTextureDescriptors()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TexturesIndexBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
        TexturesIndexStagingBuffers[i].Destroy(RendererContext->DeviceContext.logicalDevice);
    }
    MeshTexturesDescriptor.Destroy(RendererContext->DeviceContext.logicalDevice);
}
/*

void VKSCENE::Scene::WriteTexture(
    MaterialTextureType TextureType,
    VKSCENE::Mesh& Mesh,
    VKSCENE::TextureImportManager& TextureImportManager,
    std::vector<VKCORE::DescriptorSetWriteImage>& ImageWrites,
    std::vector<int>& TextureIndexes,
    int& CurrentImageIndex,
    uint32_t FrameIndex
)
{
    auto TextureIndex = Mesh.MeshMaterial.GetTexture(TextureType);
    if (TextureIndex)
    {
        auto& Iterator = TextureImportManager.TextureDatas.find(TextureIndex);
        if (Iterator == TextureImportManager.TextureDatas.end())
        {
            TextureIndexes.push_back(-1);
            return;
        }
        auto& TextureData = TextureImportManager.TextureDatas[TextureIndex];

        VKCORE::DescriptorSetWriteImage NewTextureWrite(
            TextureData.ImageView,
            TextureData.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            MeshTexturesDescriptor.DescriptorSets[FrameIndex],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            CurrentImageIndex,
            1
        );
        ImageWrites.push_back(std::move(NewTextureWrite));
        TextureIndexes.push_back(CurrentImageIndex);
        CurrentImageIndex++;
    }
    else TextureIndexes.push_back(-1);
}

void VKSCENE::Scene::UpdateTextureDescriptors(VKSCENE::TextureImportManager &TextureImportManager,uint32_t FrameIndex)
{
    std::vector<VKCORE::DescriptorSetWriteImage> ImageWrites;
    std::vector<int> TextureIndexes;
    int CurrentImageIndex = 0;
    for (auto& [ModelPtr,ModelEntry] : MeshBuffers.ModelEntries[FrameIndex].ModelEntries)
    {
        for (auto& Mesh : ModelPtr->Meshes)
        {
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_ALBEDO,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex,
                FrameIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_ROUGHNESS,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex,
                FrameIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_METALLIC,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex,
                FrameIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_NORMAL_MAP,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex,
                FrameIndex
            );
            WriteTexture(
                MATERIAL_TEXTURE_TYPE_OPACITY,
                Mesh,
                TextureImportManager,
                ImageWrites,
                TextureIndexes,
                CurrentImageIndex,
                FrameIndex
            );
        }
    }

    VKCORE::WriteDescriptorSets(
        RendererContext->DeviceContext.logicalDevice, 
        {}, 
        ImageWrites
    );    

    VkDeviceSize BufferSize = sizeof(int) * TextureIndexes.size();
    void* DataPtr;
    std::vector<VKCORE::BufferCopyInfo> CopyInfos;
    for (size_t i = 0; i < TexturesIndexStagingBuffers.size(); i++)
    {
        vkMapMemory(
            RendererContext->DeviceContext.logicalDevice,
            TexturesIndexStagingBuffers[i].BufferMemory,
            0,
            BufferSize,
            0,
            &DataPtr
        );
        memcpy(DataPtr, TextureIndexes.data(), BufferSize);
        vkUnmapMemory(RendererContext->DeviceContext.logicalDevice, TexturesIndexStagingBuffers[i].BufferMemory);

        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = 0;
        CopyRegion.size = BufferSize;
        CopyInfos.push_back({{ CopyRegion }, TexturesIndexStagingBuffers[i].BufferObject, TexturesIndexBuffers[i].BufferObject});
    }
    
    VKCORE::CopyBuffer(
        CopyInfos,
        RendererContext->DeviceContext.logicalDevice,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

}
*/

