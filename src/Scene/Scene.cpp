#include "Scene.hpp"
#include "Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../Renderer/RendererContext.hpp"
#include "Cubemap.hpp"
#include "../Renderer/Core/VulkanPipeline.hpp"

#include "MaterialManager.hpp"

SCENE::Scene::Scene(RENDERER::RendererContext& RendererContext,TextureImportManager &Manager, MeshManager& MeshManager)
{
    Create(RendererContext, Manager, MeshManager);
}

void SCENE::Scene::Create(RENDERER::RendererContext& RendererContext,TextureImportManager& Manager, MeshManager& MeshManager)
{
    this->MeshBuffers.Create(MeshManager,RendererContext);
    this->LightManager.Create(RendererContext);
    TextureManager = &Manager;
    this->MeshManagerPtr = &MeshManager;
   
    //SceneDescriptorSetLayout + IndirectDescriptorSetLayout + TextureIndicesDescriptorSetLayout
    SceneDescriptorPool.Create(
        { 
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9 * MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * MAX_FRAMES_IN_FLIGHT}
        },
        10 * MAX_FRAMES_IN_FLIGHT, 
        RendererContext.DeviceContext.logicalDevice
    );
    
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.SceneDescriptorSetLayout.descriptorSetLayout, SceneDescriptorSets.data());
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.IndirectDescriptorSetLayout.descriptorSetLayout, IndirectDescriptorSets.data());
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.logicalDevice, MAX_FRAMES_IN_FLIGHT, SceneDescriptorPool.descriptorPool, RendererContext.TextureIndicesDescriptorSetLayout.descriptorSetLayout, TextureIndicesDescriptorSets.data());

    this->RendererContext = &RendererContext;
    DrawCubeMap = true;
    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);
}

void SCENE::Scene::Destroy()
{
    if(IsDestroyed) return;

    SceneDescriptorPool.Destroy(RendererContext->DeviceContext.logicalDevice);
    DestroyMeshBuffers();
    //DestroyLightBuffers();
    LightManager.Destroy(RendererContext->DeviceContext.logicalDevice);
    for (size_t i = 0; i < StagingBuffers.size(); i++)
    {
        StagingBuffers[i].StagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    }
    IsDestroyed = true;

    std::cout << "Scene destroyed!" << std::endl;
}

void SCENE::Scene::LinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesAppendList[i].push_back(&Instance);
        ModelInstancesTransformationUpdateList[i].push_back(&Instance);
    }
}

void SCENE::Scene::LinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesAppendList[i].insert(ModelInstancesAppendList[i].end(), Instances.begin(), Instances.end());
    }
}

void SCENE::Scene::UnlinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesEraseList[i].push_back(&Instance);
    }
}

void SCENE::Scene::UnlinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        ModelInstancesEraseList[i].insert(ModelInstancesEraseList[i].end(), Instances.begin(), Instances.end());
    }
}

void SCENE::Scene::LinkCubemap(Cubemap& DestinationCubeMap)
{
    SceneCubeMap = &DestinationCubeMap;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage CubemapTextureWrite(SceneCubeMap->ConvolutionSampleImageView, SceneCubeMap->ConvolutionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, {}, { CubemapTextureWrite });
    }
}

void SCENE::Scene::LinkCamera(Camera3D& Camera)
{
    this->Camera = &Camera;
}

void SCENE::Scene::DestroyMeshBuffers()
{
    MeshBuffers.Destroy(RendererContext->DeviceContext.logicalDevice);
}

void SCENE::Scene::MarkResourceChanged(Resource* Resource, MarkChangedType Type, uint32_t FrameIndex)
{
    bool IsAllFrames = FrameIndex == std::numeric_limits<uint32_t>::max();
    if (!IsAllFrames && FrameIndex >= MAX_FRAMES_IN_FLIGHT) throw std::runtime_error("Given Frame Index is larger than the maximum frames in flight count!");

    for (uint32_t i = (IsAllFrames ? 0 : FrameIndex); i < (IsAllFrames ? MAX_FRAMES_IN_FLIGHT : (FrameIndex + 1)); i++)
    {
        if (Type & MARK_CHANGED_TYPE_MESH_TRANSFORMATION)
        {
            ModelInstancesTransformationUpdateList[i].push_back(reinterpret_cast<ModelInstance*>(Resource));
        }
        if (Type & MARK_CHANGED_TYPE_DYNAMIC_LIGHT)
        {
            DynamicLightAppendUpdateList[i].push_back(reinterpret_cast<Light*>(Resource));
        }
        if (Type & MARK_CHANGED_TYPE_STATIC_LIGHT)
        {
            StaticLightAppendUpdateList[i].push_back(reinterpret_cast<Light*>(Resource));
        }
    }
}

void SCENE::Scene::FlushPendingUpdates(SceneUpdateType Type, uint32_t FrameIndex)
{
    if (!Type) return;

    bool IsAllFrames = FrameIndex == std::numeric_limits<uint32_t>::max();
    if (!IsAllFrames && FrameIndex >= MAX_FRAMES_IN_FLIGHT) throw std::runtime_error("Given Frame Index is larger than the maximum frames in flight count!");

    //Update all the frames if IsAllFrames
    for (uint32_t i = (IsAllFrames ? 0 : FrameIndex); i < (IsAllFrames ? MAX_FRAMES_IN_FLIGHT : (FrameIndex + 1)); i++)
    {
        if ((Type & SCENE_UPDATE_TYPE_LINK_MESHES) && !ModelInstancesAppendList[i].empty())
        {
            bool IsModelMatrixBufferReallocated = false;

            MeshBuffers.AppendModels(
                ModelInstancesAppendList[i],
                i, 
                IndirectDescriptorSets, 
                StagingBuffers[i], 
                SceneCopyInfos[i]
            );

            ModelInstancesAppendList[i].clear();
        }
        if ((Type & SCENE_UPDATE_TYPE_UNLINK_MESHES) && !ModelInstancesEraseList[i].empty())
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
            Info.TargetDescriptorSets = TextureIndicesDescriptorSets;
            Info.TextureImportManagerPtr = this->TextureManager;
            Info.CopyInfos = &SceneCopyInfos[i];
            Info.StagingBuffer = &StagingBuffers[i];

            MeshBuffers.UpdateTextureDescriptors(Info);
        }
        if ((Type & SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS) || (Type & SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS))
        {
            std::vector<Light*> EmptyLightList;
            LightAppendOrUpdateInfo Info{};
            Info.FrameIndex = i;
            Info.DynamicLights = (Type & SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS) ? DynamicLightAppendUpdateList[i] : EmptyLightList;
            Info.StaticLights = (Type & SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS) ? StaticLightAppendUpdateList[i] : EmptyLightList;
            Info.TargetDescriptorSets = SceneDescriptorSets;
            LightManager.AppendOrUpdateLights(Info);
        }
    }
}

void SCENE::Scene::UpdateMeshTransformations(uint32_t CurrentFrame)
{
    this->MeshBuffers.UpdateMeshTransformations(ModelInstancesTransformationUpdateList[CurrentFrame], CurrentFrame);
    ModelInstancesTransformationUpdateList[CurrentFrame].clear();
}

void SCENE::Scene::LinkDynamicLight(Light& DynamicLight)
{
    for (size_t i = 0; i < DynamicLightAppendUpdateList.size(); i++)
    {
        DynamicLightAppendUpdateList[i].push_back(&DynamicLight);
    }
}

void SCENE::Scene::LinkStaticLight(Light& StaticLight)
{
    for (size_t i = 0; i < StaticLightAppendUpdateList.size(); i++)
    {
        StaticLightAppendUpdateList[i].push_back(&StaticLight);
    }
}

void SCENE::Scene::LinkDynamicLight(std::vector<Light*>& DynamicLights)
{
    for (size_t i = 0; i < DynamicLightAppendUpdateList.size(); i++)
    {
        DynamicLightAppendUpdateList[i].insert(DynamicLightAppendUpdateList[i].begin(), DynamicLightAppendUpdateList[i].end(), DynamicLights.begin());
    }
}

void SCENE::Scene::LinkStaticLight(std::vector<Light*>& StaticLights)
{
    for (size_t i = 0; i < StaticLightAppendUpdateList.size(); i++)
    {
        StaticLightAppendUpdateList[i].insert(StaticLightAppendUpdateList[i].begin(), StaticLightAppendUpdateList[i].end(), StaticLights.begin());
    }
}

