#include "Scene.hpp"
#include "Mesh.hpp"
#include "../Renderer/Core/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "../Renderer/RendererContext.hpp"
#include "Cubemap.hpp"
#include "../Renderer/Core/VulkanPipeline.hpp"

#include "../Renderer/ResourceManager.hpp"
#include <chrono>

SCENE::Scene::Scene(RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManager,SceneOptions Options)
{
    Create(RendererContext, ResourceManager);
}

void SCENE::Scene::Create(RENDERER::RendererContext& RendererContext,RENDERER::ResourceManager& ResourceManager,SceneOptions Options)
{
    this->Options = Options;
    this->MeshBuffers.Create(ResourceManager,RendererContext, Options.BufferAllocationStep);
    this->LightManager.Create(RendererContext);
    this->ResourceManagerPtr = &ResourceManager;

    PendingUpdateBits.fill(SCENE_UPDATE_TYPE_NONE);
    //SceneDescriptorSetLayout + IndirectDescriptorSetLayout + TextureIndicesDescriptorSetLayout
    SceneDescriptorPool.Create(
        { 
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9 * MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * MAX_FRAMES_IN_FLIGHT}
        },
        10 * MAX_FRAMES_IN_FLIGHT, 
        RendererContext.DeviceContext.LogicalDevice
    );
    
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.LogicalDevice, MAX_FRAMES_IN_FLIGHT, 
        SceneDescriptorPool.Handle, RendererContext.SceneDescriptorSetLayout.Handle, SceneDescriptorSets.data());
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.LogicalDevice, MAX_FRAMES_IN_FLIGHT,
        SceneDescriptorPool.Handle, RendererContext.IndirectDescriptorSetLayout.Handle, IndirectDescriptorSets.data());
    RENDERER_CORE::AllocateDescriptorSets(RendererContext.DeviceContext.LogicalDevice, MAX_FRAMES_IN_FLIGHT,
        SceneDescriptorPool.Handle, RendererContext.TextureIndicesDescriptorSetLayout.Handle, TextureIndicesDescriptorSets.data());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        auto& CurrentCopyOperationIndices = SceneCopyInfoIndices[i];
       /* CurrentCopyOperationIndices[INDIRECT_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            MeshBuffers.SceneBuffers.IndirectBuffers[i].Buffer.BufferObject,
            i,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
        );

        CurrentCopyOperationIndices[DRAWMETA_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            MeshBuffers.SceneBuffers.DrawMetaDataBuffer[i].Buffer.BufferObject,
            i,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );

        CurrentCopyOperationIndices[TEXTUREINDEX_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            MeshBuffers.SceneBuffers.TexturesIndexBuffers[i].Buffer.BufferObject,
            i,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );

        CurrentCopyOperationIndices[TRANSFORMATION_MATRIX_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            MeshBuffers.SceneBuffers.ModelMatricesBuffers[i].Buffer.BufferObject,
            i,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT
        );*/

        CurrentCopyOperationIndices[INDIRECT_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &MeshBuffers.Buffers.IndirectBuffers[i].Buffer,
            i
        );

        CurrentCopyOperationIndices[DRAWMETA_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &MeshBuffers.Buffers.DrawMetaDataBuffer[i].Buffer,
            i
        );

        CurrentCopyOperationIndices[TEXTUREINDEX_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &MeshBuffers.Buffers.TexturesIndexBuffers[i].Buffer,
            i
        );

        CurrentCopyOperationIndices[TRANSFORMATION_MATRIX_COPY] = ResourceManager.RequestCopyOperation(
            RENDERER_CORE::QUEUE_TYPE_GRAPHICS,
            &MeshBuffers.Buffers.ModelMatricesBuffers[i].Buffer,
            i
        );
    }

    this->RendererContext = &RendererContext;
    DrawCubeMap = true;
    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);
}

void SCENE::Scene::Destroy()
{
    if(IsDestroyed) return;

    SceneDescriptorPool.Destroy(RendererContext->DeviceContext.LogicalDevice);
    DestroyMeshBuffers();
    //DestroyLightBuffers();
    LightManager.Destroy(RendererContext->DeviceContext.LogicalDevice);
    IsDestroyed = true;

    std::cout << "Scene destroyed!" << std::endl;
}

void SCENE::Scene::LinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].ModelInstancesAppendList.push_back(&Instance);
        UpdateLists[i].ModelInstancesTransformationUpdateList.push_back(&Instance);
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS | 
            SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS | SCENE_UPDATE_TYPE_LINK_MESHES;
    }
}

void SCENE::Scene::LinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].ModelInstancesAppendList.insert(UpdateLists[i].ModelInstancesAppendList.end(), Instances.begin(), Instances.end());
        UpdateLists[i].ModelInstancesTransformationUpdateList.insert(UpdateLists[i].ModelInstancesTransformationUpdateList.end(), Instances.begin(), Instances.end());
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS | 
            SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS | SCENE_UPDATE_TYPE_LINK_MESHES;
    }
}

void SCENE::Scene::UnlinkModelInstance(ModelInstance& Instance)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].ModelInstancesEraseList.push_back(&Instance);
    }
}

void SCENE::Scene::UnlinkModelInstance(std::vector<ModelInstance*>& Instances)
{
    for (uint16_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].ModelInstancesEraseList.insert(UpdateLists[i].ModelInstancesEraseList.end(), Instances.begin(), Instances.end());
    }
}

void SCENE::Scene::LinkCubemap(Cubemap& DestinationCubeMap)
{
    SceneCubeMap = &DestinationCubeMap;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        RENDERER_CORE::DescriptorSetWriteImage CubemapTextureWrite(SceneCubeMap->ConvolutionSampleImageView, SceneCubeMap->ConvolutionSampler, 
                                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, SceneDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.LogicalDevice, {}, { CubemapTextureWrite });
    }
}

void SCENE::Scene::DestroyMeshBuffers()
{
    MeshBuffers.Destroy(RendererContext->DeviceContext.LogicalDevice);
}

void SCENE::Scene::MarkResourceChanged(COMMON::Handle* Resource, MarkChangedType Type, uint32_t FrameIndex)
{
    bool IsAllFrames = FrameIndex == std::numeric_limits<uint32_t>::max();
    if (!IsAllFrames && FrameIndex >= MAX_FRAMES_IN_FLIGHT) throw std::runtime_error("Given Frame Index is larger than the maximum frames in flight count!");

    for (uint32_t i = (IsAllFrames ? 0 : FrameIndex); i < (IsAllFrames ? MAX_FRAMES_IN_FLIGHT : (FrameIndex + 1)); i++)
    {
        auto& Lists = UpdateLists[i];
        auto& UpdateBit = PendingUpdateBits[i];
        if (Type & MARK_CHANGED_TYPE_MESH_TRANSFORMATION)
        {
            Lists.ModelInstancesTransformationUpdateList.push_back(reinterpret_cast<ModelInstance*>(Resource));
            UpdateBit = UpdateBit | SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS;
        }
        if (Type & MARK_CHANGED_TYPE_MESH_MATERIAL)
        {
            Lists.MaterialUpdateList.push_back(reinterpret_cast<ModelInstance*>(Resource));
            UpdateBit = UpdateBit | SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS;
        }
        if (Type & MARK_CHANGED_TYPE_DYNAMIC_LIGHT)
        {
            Lists.DynamicLightAppendUpdateList.push_back(reinterpret_cast<Light*>(Resource));
            UpdateBit = UpdateBit | SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS;
        }
        if (Type & MARK_CHANGED_TYPE_STATIC_LIGHT)
        {
            Lists.StaticLightAppendUpdateList.push_back(reinterpret_cast<Light*>(Resource));
            UpdateBit = UpdateBit | SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS;
        }
    }
}

void SCENE::Scene::FlushPendingUpdates(SceneUpdateType Type, uint32_t FrameIndex)
{
    if (!Type) return;

    bool IsAllFrames = FrameIndex == std::numeric_limits<uint32_t>::max();
    if (!IsAllFrames && FrameIndex >= MAX_FRAMES_IN_FLIGHT) throw std::runtime_error("Given Frame Index is larger than the maximum frames in flight count!");

    bool IsPendingOnly = (Type == SCENE_UPDATE_TYPE_ALL_PENDING);
    //Update all the frames if IsAllFrames
    for (uint32_t i = (IsAllFrames ? 0 : FrameIndex); i < (IsAllFrames ? MAX_FRAMES_IN_FLIGHT : (FrameIndex + 1)); i++)
    {
        //Construct the array that points to the copy operations related to this scene
        std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)> CopyOperations = {
            ResourceManagerPtr->GetCopyOperationEntry(SceneCopyInfoIndices[i][INDIRECT_COPY],i),
            ResourceManagerPtr->GetCopyOperationEntry(SceneCopyInfoIndices[i][DRAWMETA_COPY],i),
            ResourceManagerPtr->GetCopyOperationEntry(SceneCopyInfoIndices[i][TEXTUREINDEX_COPY],i),
            ResourceManagerPtr->GetCopyOperationEntry(SceneCopyInfoIndices[i][TRANSFORMATION_MATRIX_COPY],i)
        };

        auto& UpdateList = UpdateLists[i];
        const SceneUpdateType &Bit = IsPendingOnly ? this->PendingUpdateBits[i] : Type;
        if ((IsPendingOnly && (this->PendingUpdateBits[i] == SCENE_UPDATE_TYPE_NONE))) continue;
        if ((Bit & SCENE_UPDATE_TYPE_LINK_MESHES) && !UpdateList.ModelInstancesAppendList.empty())
        {
            bool IsModelMatrixBufferReallocated = false;

            MeshBuffers.AppendModels(
                UpdateList.ModelInstancesAppendList,
                UpdateList.MaterialUpdateList,
                i, 
                IndirectDescriptorSets, 
                ResourceManagerPtr->StagingBuffers[i],
                this->Options,
                CopyOperations
            );
            ResourceManagerPtr->SetCopyOperationDirty(SceneCopyInfoIndices[i][INDIRECT_COPY], i);
            ResourceManagerPtr->SetCopyOperationDirty(SceneCopyInfoIndices[i][DRAWMETA_COPY], i);

            UpdateList.ModelInstancesAppendList.clear();
        }
        if ((Bit & SCENE_UPDATE_TYPE_UNLINK_MESHES) && !UpdateList.ModelInstancesEraseList.empty())
        {
            MeshEraseInfo Info{};
            Info.FrameIndex = i;
            Info.ModelInstances = UpdateList.ModelInstancesEraseList;
            Info.TargetDescriptorSets = IndirectDescriptorSets;
            MeshBuffers.EraseModels(Info);

            UpdateList.ModelInstancesEraseList.clear();
        }
        if (Bit & SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS)
        {
            UpdateMeshTransformations(i, CopyOperations);
            ResourceManagerPtr->SetCopyOperationDirty(SceneCopyInfoIndices[i][TRANSFORMATION_MATRIX_COPY], i);
        }
        if (Bit & SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS)
        {
            MeshBuffers.UpdateMaterials(
                UpdateList.MaterialUpdateList,
                i, 
                TextureIndicesDescriptorSets[i], 
                ResourceManagerPtr->StagingBuffers[i],
                CopyOperations
            );
            ResourceManagerPtr->SetCopyOperationDirty(SceneCopyInfoIndices[i][TEXTUREINDEX_COPY], i);
            UpdateList.MaterialUpdateList.clear();
        }
        if ((Bit & SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS) || (Bit & SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS))
        {
            bool UpdateDynamicLightBuffers = (Bit & SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS);
            bool UpdateStaticLightBuffers = (Bit & SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS);

            std::vector<Light*> EmptyLightList;
            LightAppendOrUpdateInfo Info{};
            Info.FrameIndex = i;
            Info.DynamicLights = UpdateDynamicLightBuffers ? UpdateList.DynamicLightAppendUpdateList : EmptyLightList;
            Info.StaticLights = UpdateStaticLightBuffers ? UpdateList.StaticLightAppendUpdateList : EmptyLightList;
            Info.TargetDescriptorSets = SceneDescriptorSets;
            LightManager.AppendOrUpdateLights(Info);

            if (UpdateDynamicLightBuffers) UpdateList.DynamicLightAppendUpdateList.clear();
            if (UpdateStaticLightBuffers) UpdateList.StaticLightAppendUpdateList.clear();
        }
        this->PendingUpdateBits[i] = SCENE_UPDATE_TYPE_NONE;
    }
}

void SCENE::Scene::UpdateMeshTransformations(
    uint32_t CurrentFrame,
    std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)>& CopyOperations
)
{
    switch (Options.UploadMode)
    {
    case SCENE_DYNAMIC_UPLOAD_MODE_HOST_VISIBLE:
    {
        this->MeshBuffers.UpdateMeshTransformationsHostVisible(UpdateLists[CurrentFrame].ModelInstancesTransformationUpdateList, CurrentFrame);
        break;
    }
    case SCENE_DYNAMIC_UPLOAD_MODE_DEVICE_LOCAL:
    {
        this->MeshBuffers.UpdateMeshTransformationsDeviceLocal(
            UpdateLists[CurrentFrame].ModelInstancesTransformationUpdateList,
            CurrentFrame,
            CopyOperations,
            ResourceManagerPtr->StagingBuffers[CurrentFrame]
        );
        break;
    }
    default:
        break;
    }
    UpdateLists[CurrentFrame].ModelInstancesTransformationUpdateList.clear();
}

void SCENE::Scene::LinkDynamicLight(Light& DynamicLight)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].DynamicLightAppendUpdateList.push_back(&DynamicLight);
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS;
    }
}

void SCENE::Scene::LinkStaticLight(Light& StaticLight)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].StaticLightAppendUpdateList.push_back(&StaticLight);
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS;
    }
}

void SCENE::Scene::LinkDynamicLight(std::vector<Light*>& DynamicLights)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].DynamicLightAppendUpdateList.insert(UpdateLists[i].DynamicLightAppendUpdateList.begin(), UpdateLists[i].DynamicLightAppendUpdateList.end(), DynamicLights.begin());
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS;
    }
}

void SCENE::Scene::LinkStaticLight(std::vector<Light*>& StaticLights)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateLists[i].StaticLightAppendUpdateList.insert(UpdateLists[i].StaticLightAppendUpdateList.begin(), UpdateLists[i].StaticLightAppendUpdateList.end(), StaticLights.begin());
        this->PendingUpdateBits[i] = this->PendingUpdateBits[i] | SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS;
    }
}

