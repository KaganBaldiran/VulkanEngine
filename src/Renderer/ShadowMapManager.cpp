#include "ShadowMapManager.hpp"
#include "../Scene/Light.hpp"

#include <array>
#include <string>

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Renderer/MeshManager.hpp"
#include "../Renderer/Core/VulkanDescriptorSet.hpp"
#include "../Renderer/ResourceManager.hpp"

std::array<glm::vec3, 8> RENDERER::GetCameraFrustum(glm::mat4 InverseProjectMatrix, glm::mat4 InverseViewMatrix)
{
    glm::mat4 InvCameraMatrix = InverseProjectMatrix * InverseViewMatrix;
    std::array<glm::vec3,8> NDCCorners = {
        glm::vec3(- 1.0f, 1.0f, -1.0f),
        glm::vec3(1.0f, 1.0f, -1.0f),
        glm::vec3(1.0f, -1.0f, -1.0f),
        glm::vec3(- 1.0f, -1.0f, -1.0f),
        glm::vec3(-1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f),
        glm::vec3(-1.0f, -1.0f, 1.0f)
    };

    for (size_t i = 0; i < NDCCorners.size(); i++)
    {
        glm::vec4 WorldSpaceCorner = (InvCameraMatrix * glm::vec4(NDCCorners[i], 1.0f));
        NDCCorners[i] = WorldSpaceCorner / WorldSpaceCorner.w;
    }
    return NDCCorners;
}

glm::mat4 RENDERER::GetLightSpaceMatrix(glm::vec3 LightDirection,std::array<glm::vec3, 8>& Frustum)
{
    glm::vec3 Center = glm::vec3(0.0f);
    for (auto& Corner : Frustum)
    {
        Center += Corner;
    }
    Center /= static_cast<float>(Frustum.size());
    glm::mat4 LightViewMatrix = glm::lookAt(Center + LightDirection, Center, { 0.0f,1.0f,0.0f });

    float MinX = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::lowest();
    float MinY = std::numeric_limits<float>::max();
    float MaxY = std::numeric_limits<float>::lowest();
    float MinZ = std::numeric_limits<float>::max();
    float MaxZ = std::numeric_limits<float>::lowest();
    for (auto& Corner : Frustum)
    {
        glm::vec3 TransformedCorner = LightViewMatrix * glm::vec4(Corner,1.0f);
        MinX = glm::min(MinX, TransformedCorner.x);
        MaxX = glm::max(MaxX, TransformedCorner.x);
        MinY = glm::min(MinY, TransformedCorner.y);
        MaxY = glm::max(MaxY, TransformedCorner.y);
        MinZ = glm::min(MinZ, TransformedCorner.z);
        MaxZ = glm::max(MaxZ, TransformedCorner.z);
    }

    constexpr float Zmultiplier = 10.0f; 
    if (MinZ < 0) MinZ *= Zmultiplier;
    else MinZ /= Zmultiplier;
    if (MaxZ < 0) MaxZ /= Zmultiplier;
    else MaxZ *= Zmultiplier;

    glm::mat4 LightProjection = glm::ortho(MinX, MaxX, MinY, MaxY, MinZ, MaxZ);
    return LightProjection * LightViewMatrix;
}

RENDERER::TexturePacker3D::TexturePacker3D(glm::ivec2 PageSize)
{
    Create(PageSize);
}

void RENDERER::TexturePacker3D::Create(glm::ivec2 PageSize)
{
    this->PageSize = PageSize;
    Pages.reserve(100);

    TextureLayer Layer;
    Layer.FreePixelCount = static_cast<size_t>(PageSize.x) * PageSize.y;
    Pages.push_back(Layer);
}

RENDERER::MemoryRegion3D RENDERER::TexturePacker3D::Insert(glm::ivec2 Size)
{
    if (Size.x > PageSize.x || Size.y > PageSize.y) return MemoryRegion3D();
    for (size_t PageIndex = 0; PageIndex < Pages.size(); PageIndex++)
    {
        auto& Page = Pages[PageIndex];
        if (Page.FreePixelCount < (static_cast<size_t>(Size.x) * Size.y))
        {
            if (PageIndex == (Pages.size() - 1))
            {
                TextureLayer Layer;
                Layer.FreePixelCount = static_cast<size_t>(PageSize.x) * PageSize.y;
                Pages.push_back(Layer);
            }
            else continue;
        }
        auto& PageRegions = Page.Regions;
        
        std::vector<const MemoryRegion2D*> RegionPtrs;
        std::vector<glm::vec2> CandidatePositions;
        CandidatePositions.push_back({ 0,0 });
        for (auto& Region : PageRegions)
        {
            CandidatePositions.push_back(Region.Offset);
            CandidatePositions.push_back({ Region.Offset.x,Region.Offset.y + Region.Size.y });
            CandidatePositions.push_back({ Region.Offset.x + Region.Size.x,Region.Offset.y });
            CandidatePositions.push_back(Region.Offset + Region.Size);

            RegionPtrs.push_back(&Region);
        }
        std::sort(CandidatePositions.begin(), CandidatePositions.end(), [](const glm::vec2& Candidate0, const glm::vec2& Candidate1) {
            if (Candidate0.y != Candidate1.y) return Candidate0.y < Candidate1.y;
            return Candidate0.x < Candidate1.x;
        });

        auto Iterator = std::unique(CandidatePositions.begin(), CandidatePositions.end());
        CandidatePositions.resize(std::distance(CandidatePositions.begin(), Iterator));

        for (size_t CandidateIndex = 0; CandidateIndex < CandidatePositions.size(); CandidateIndex++)
        {
            auto &Position = CandidatePositions[CandidateIndex];
            if ((Position.x + Size.x) <= PageSize.x && (Position.y + Size.y) <= PageSize.y)
            {
                auto OverlappingRegion = TextureLayer::DoesOverlap(RegionPtrs, Position.x, Position.y, Position.x + Size.x, Position.y + Size.y);
                if (!OverlappingRegion)
                {
                    MemoryRegion3D NewRegion{};
                    NewRegion.Layer = PageIndex;
                    NewRegion.Region = {Size,Position };
                    Page.Regions.insert(NewRegion.Region);

                    Page.FreePixelCount -= static_cast<size_t>(Size.x) * Size.y;
                    return NewRegion;
                }
            }
        }
    } 
    return MemoryRegion3D();
}

void RENDERER::TexturePacker3D::Erase(const MemoryRegion3D& Region)
{
    if (Region.Layer >= Pages.size()) return;
    auto& Page = Pages[Region.Layer];
    auto Iterator = Page.Regions.find(Region.Region);
    if (Iterator == Page.Regions.end())
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Attempting to erase an nonexistent 3D memory region [offset(" 
                                        + std::to_string(Region.Region.Offset.x) + "," + std::to_string(Region.Region.Offset.y) + 
                                        ")size(" + std::to_string(Region.Region.Size.x) + "," + std::to_string(Region.Region.Size.y) + ")]."));
        throw std::runtime_error("Attempting to erase an nonexistent 3D memory region.");
    }
    Page.FreePixelCount += Region.Region.Size.x * Region.Region.Size.y;
    Page.Regions.erase(Region.Region);
}

const RENDERER::MemoryRegion2D* RENDERER::TextureLayer::DoesOverlap(
    const std::vector<const MemoryRegion2D*>& Regions,
    float Min_x, 
    float Min_y, 
    float Max_x, 
    float Max_y
)
{
    for (auto& RegionPtr : Regions)
    {
        if ((RegionPtr->Offset.x < Max_x && (RegionPtr->Offset.x + RegionPtr->Size.x) > Min_x) &&
            (RegionPtr->Offset.y < Max_y && (RegionPtr->Offset.y + RegionPtr->Size.y) > Min_y))
        {
            return { RegionPtr };
        }
    }
    return nullptr;
}

size_t RENDERER::MemoryRegion2DHasher::operator()(const MemoryRegion2D& Region) const
{
    size_t seed = 0;
    size_t Hash0 = std::hash<int>()(Region.Size.x);
    size_t Hash1 = std::hash<int>()(Region.Size.y);
    size_t Hash2 = std::hash<int>()(Region.Offset.x);
    size_t Hash3 = std::hash<int>()(Region.Offset.y);
    seed ^= Hash0 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= Hash1 + 0x8e37754a + (seed << 6) + (seed >> 2);
    seed ^= Hash2 + 0x1e9185b9 + (seed << 6) + (seed >> 2);
    seed ^= Hash3 + 0x7e2749b9 + (seed << 6) + (seed >> 2);
    return seed;
}

RENDERER::ShadowMapManager::ShadowMapManager(RENDERER::RendererContext& RendererContext, glm::ivec2 PageSize)
{
    Create(RendererContext,PageSize);
}

void RENDERER::ShadowMapManager::Create(RENDERER::RendererContext& RendererContext, glm::ivec2 PageSize)
{
    this->RendererContext = &RendererContext;
    this->PageSize = PageSize;
    this->LayerCount.fill(10);
    for (auto& CurrentShadowMapTextures : ShadowMapTextures)
    {
        CurrentShadowMapTextures.Samplers.emplace_back();
    }
}

void RENDERER::ShadowMapManager::Destroy()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CascadedShadowMapsDataBuffers[i].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
        CascadedShadowMapsMetaDataBuffers[i].Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
        ShadowMapTextures[i].Destroy(RendererContext->DeviceContext.LogicalDevice);
    }
}

void RENDERER::ShadowMapManager::AppendCascadedShadowMap(
    std::vector<CascadedShadowMapInfo>& Infos,
    uint32_t FrameIndex,
    std::array<RENDERER::CopyOperationEntry*, 2>& CopyInfos,
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& TargetDescriptorSets,
    SCENE::PersistentStagingBuffer& StagingBuffer
)
{
    auto& CurrentMetaDataBuffer = CascadedShadowMapsMetaDataBuffers[FrameIndex];
    auto& CurrentDataBuffer = CascadedShadowMapsDataBuffers[FrameIndex];
    auto& CurrentMetaDataBufferAllocator = CurrentMetaDataBuffer.Allocator;
    auto& CurrentDataBufferAllocator = CurrentDataBuffer.Allocator;
    auto& TargetDescriptorSet = TargetDescriptorSets[FrameIndex];

    TexturePacker3D& CurrentTexturePacker = TexturePackers[FrameIndex];
    size_t InitialPageCount = CurrentTexturePacker.GetPageCount();

    auto& CurrentShadowMapEntries = this->CascadedShadowMapEntries[FrameIndex];
    size_t SizeOfMetaData = sizeof(CascadedMapMetaData), SizeOfData = sizeof(CascadedMapData);
    size_t InitialCascadeDataBufferCapacity = CurrentTexturePacker.GetPageCount();

    size_t InitialMetaDataBufferCapacity = CurrentMetaDataBufferAllocator.GetCapacity();
    size_t InitialMetaDataBufferUsedSpace = CurrentMetaDataBufferAllocator.GetUsedSpace();
    size_t InitialDataBufferCapacity = CurrentDataBufferAllocator.GetCapacity();
    size_t InitialDataBufferUsedSpace = CurrentDataBufferAllocator.GetUsedSpace();

    for (auto& InputInfo : Infos)
    {
        if (!InputInfo.SourceLight) continue;

        auto Iterator = CurrentShadowMapEntries.find(InputInfo.SourceLight->GetHandleID());
        if (Iterator != CurrentShadowMapEntries.end())
        {
            continue;
        }

        size_t CascadeCount = InputInfo.Cascades.size();
        RENDERER_CORE::MemoryRegion DataBatchRegion = CurrentDataBufferAllocator.Suballocate(SizeOfData * CascadeCount);

        CascadedMapMetaData MetaData{};
        MetaData.CascadeCount = CascadeCount;
        MetaData.Offset = DataBatchRegion.Offset / SizeOfMetaData;
        MetaData.LightDirection = InputInfo.SourceLight->Data.PositionOrDirection;

        CascadedShadowMapEntry NewShadowMapEntry{};
        NewShadowMapEntry.MetaDataMemoryRegion = CurrentMetaDataBufferAllocator.Suballocate(SizeOfMetaData);
        NewShadowMapEntry.MetaData = MetaData;
        NewShadowMapEntry.CascadeEntries.reserve(CascadeCount);
        for (size_t i = 0; i < CascadeCount; i++)
        {
            const ShadowMapCascade& Cascade = InputInfo.Cascades[i];

            MemoryRegion3D NewTextureRegion = CurrentTexturePacker.Insert(Cascade.TextureSize);
            CascadedMapData NewCascadeData{};
            NewCascadeData.TextureSize = NewTextureRegion.Region.Size.x;
            NewCascadeData.TexturePosition = NewTextureRegion.Region.Offset;
            NewCascadeData.TextureLayer = NewTextureRegion.Layer;
            NewCascadeData.Distance = Cascade.Distance;
            NewCascadeData.ShadowCascadeLevel = i;

            CascadeEntry NewEntry{};
            NewEntry.TextureRegion = NewTextureRegion;
            NewEntry.MemoryRegion = { DataBatchRegion.Offset + i * SizeOfData,SizeOfData };
            NewEntry.Data = NewCascadeData;
            NewShadowMapEntry.CascadeEntries.push_back(NewEntry);
        }
        auto EntryReference = CurrentShadowMapEntries.insert({ InputInfo.SourceLight->GetHandleID() ,std::move(NewShadowMapEntry) });
    }
    bool MetaDataBufferReallocated = CurrentMetaDataBufferAllocator.GetCapacity() > InitialMetaDataBufferCapacity;
    bool DataBufferReallocated = CurrentDataBufferAllocator.GetCapacity() > InitialDataBufferCapacity;

    size_t MetaDataStagingBufferSize = MetaDataBufferReallocated ? CurrentMetaDataBufferAllocator.GetUsedSpace() :
        (CurrentMetaDataBufferAllocator.GetUsedSpace() - InitialMetaDataBufferUsedSpace);
    size_t DataStagingBufferSize = DataBufferReallocated ? CurrentDataBufferAllocator.GetCapacity() :
        (CurrentDataBufferAllocator.GetUsedSpace() - InitialDataBufferUsedSpace);
    size_t TotalStagingBufferSize = MetaDataStagingBufferSize + DataStagingBufferSize;

    CreateShadowMapBuffers(
        MetaDataBufferReallocated,
        DataBufferReallocated,
        CurrentMetaDataBuffer,
        CurrentDataBuffer,
        TargetDescriptorSet
    );
    StagingBuffer.AllocateSceneStagingBuffer(TotalStagingBufferSize, RendererContext);
    CreateShadowMapTextures(CurrentTexturePacker.GetPageCount(), FrameIndex, TargetDescriptorSet);

    bool IsThereMetaDataCopyInfos = CopyInfos[0]->CopyInfo.CopyRegions.empty();
    bool IsThereDataCopyInfos = CopyInfos[1]->CopyInfo.CopyRegions.empty();

    if (!IsThereMetaDataCopyInfos && MetaDataBufferReallocated) CopyInfos[0]->CopyInfo.CopyRegions.clear();
    if (!IsThereDataCopyInfos && DataBufferReallocated) CopyInfos[1]->CopyInfo.CopyRegions.clear();

    uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.StagingBuffer.Buffer.MappedMemory);
    if (!StagingBufferPtr) throw std::runtime_error("Unable to map the staging buffer! exitting...");

    if (MetaDataBufferReallocated || DataBufferReallocated)
    {
        for (auto& ShadowMapEntry : CurrentShadowMapEntries)
        {
            RENDERER::CascadedShadowMapEntry& Entry = ShadowMapEntry.second;
            if (MetaDataBufferReallocated || Entry.RequiresUpload)
            {
                //Should allocate a region from the staging buffer
                if (!IsThereMetaDataCopyInfos || !Entry.StagingMetaDataMemoryRegion.Size)
                {
                    Entry.StagingMetaDataMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfMetaData);
                }
                memcpy(StagingBufferPtr + Entry.StagingMetaDataMemoryRegion.Offset, &Entry.MetaData, Entry.MetaDataMemoryRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = Entry.MetaDataMemoryRegion.Offset;
                CopyRegion.size = Entry.MetaDataMemoryRegion.Size;
                CopyRegion.srcOffset = Entry.StagingMetaDataMemoryRegion.Offset;
                CopyInfos[0]->CopyInfo.CopyRegions.push_back(std::move(CopyRegion));
            }
            if (DataBufferReallocated || Entry.RequiresUpload)
            {
                for (auto& CascadeEntry : Entry.CascadeEntries)
                {
                    //Should allocate a region from the staging buffer
                    if (!IsThereDataCopyInfos || !CascadeEntry.StagingMemoryRegion.Size)
                    {
                        CascadeEntry.StagingMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfData);
                    }
                    memcpy(StagingBufferPtr + CascadeEntry.StagingMemoryRegion.Offset, &CascadeEntry.Data, CascadeEntry.StagingMemoryRegion.Size);

                    VkBufferCopy CopyRegion{};
                    CopyRegion.dstOffset = CascadeEntry.MemoryRegion.Offset;
                    CopyRegion.size = CascadeEntry.StagingMemoryRegion.Size;
                    CopyRegion.srcOffset = CascadeEntry.StagingMemoryRegion.Offset;
                    CopyInfos[1]->CopyInfo.CopyRegions.push_back(std::move(CopyRegion));
                }
            } 
            Entry.RequiresUpload = false;
        }
    }
    else
    {
        for (auto& InputInfo : Infos)
        {
            auto Iterator = CurrentShadowMapEntries.find(InputInfo.SourceLight->GetHandleID());
            if (Iterator != CurrentShadowMapEntries.end()) continue;

            RENDERER::CascadedShadowMapEntry& Entry = Iterator->second;
            if (MetaDataBufferReallocated || Entry.RequiresUpload)
            {
                //Should allocate a region from the staging buffer
                if (!IsThereMetaDataCopyInfos || !Entry.StagingMetaDataMemoryRegion.Size)
                {
                    Entry.StagingMetaDataMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfMetaData);
                }
                memcpy(StagingBufferPtr + Entry.StagingMetaDataMemoryRegion.Offset, &Entry.MetaData, Entry.MetaDataMemoryRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = Entry.MetaDataMemoryRegion.Offset;
                CopyRegion.size = Entry.MetaDataMemoryRegion.Size;
                CopyRegion.srcOffset = Entry.StagingMetaDataMemoryRegion.Offset;
                CopyInfos[0]->CopyInfo.CopyRegions.push_back(std::move(CopyRegion));
            }
            for (auto& CascadeEntry : Entry.CascadeEntries)
            {
                //Should allocate a region from the staging buffer
                if (!IsThereDataCopyInfos || !CascadeEntry.StagingMemoryRegion.Size)
                {
                    CascadeEntry.StagingMemoryRegion = StagingBuffer.StagingBuffer.Allocator.Suballocate(SizeOfData);
                }
                memcpy(StagingBufferPtr + CascadeEntry.StagingMemoryRegion.Offset, &CascadeEntry.Data, CascadeEntry.StagingMemoryRegion.Size);

                VkBufferCopy CopyRegion{};
                CopyRegion.dstOffset = CascadeEntry.MemoryRegion.Offset;
                CopyRegion.size = CascadeEntry.StagingMemoryRegion.Size;
                CopyRegion.srcOffset = CascadeEntry.StagingMemoryRegion.Offset;
                CopyInfos[1]->CopyInfo.CopyRegions.push_back(std::move(CopyRegion));
            }
            Entry.RequiresUpload = false; 
        }
    }
}

void RENDERER::ShadowMapManager::CreateShadowMapTextures(
    size_t RequestedPageCount, 
    size_t FrameIndex, 
    VkDescriptorSet TargetDescriptorSet
)
{
   auto& CurrentShadowMapTextures = ShadowMapTextures[FrameIndex];
   auto& CurrentLayerCount = LayerCount[FrameIndex];

   if (RequestedPageCount > CurrentLayerCount)
   {
       if (CurrentLayerCount) CurrentShadowMapTextures.Destroy(RendererContext->DeviceContext.LogicalDevice);
       ShadowMapImageFormat = RENDERER_CORE::FindSupportedFormat(RendererContext->DeviceContext.PhysicalDevice, { VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM },
           VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

       RENDERER_CORE::CreateImage(
           RendererContext->DeviceContext.PhysicalDevice,
           RendererContext->DeviceContext.LogicalDevice,
           PageSize.x,
           PageSize.y,
           VK_IMAGE_TILING_OPTIMAL,
           ShadowMapImageFormat,
           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
           CurrentShadowMapTextures.Image,
           CurrentShadowMapTextures.ImageMemory,
           RequestedPageCount
       );

       CurrentShadowMapTextures.ImageViews.push_back(RENDERER_CORE::CreateImageView(
           CurrentShadowMapTextures.Image,
           ShadowMapImageFormat,
           VK_IMAGE_VIEW_TYPE_2D_ARRAY,
           VK_IMAGE_ASPECT_DEPTH_BIT,
           RendererContext->DeviceContext.LogicalDevice,
           RequestedPageCount
       ));
       for (size_t i = 0; i < RequestedPageCount; i++)
       {
           CurrentShadowMapTextures.ImageViews.push_back(RENDERER_CORE::CreateImageView(
               CurrentShadowMapTextures.Image,
               ShadowMapImageFormat,
               VK_IMAGE_VIEW_TYPE_2D,
               VK_IMAGE_ASPECT_DEPTH_BIT,
               RendererContext->DeviceContext.LogicalDevice,
               1,
               i
           ));
       }
       RENDERER_CORE::CreateTextureSampler(
           RendererContext->DeviceContext.PhysicalDevice,
           RendererContext->DeviceContext.LogicalDevice,
           CurrentShadowMapTextures.Samplers[0],
           VK_FILTER_LINEAR,
           VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
       );

       RENDERER_CORE::DescriptorSetWriteImage ImageWrite(CurrentShadowMapTextures.ImageViews[0],
           CurrentShadowMapTextures.Samplers[0],
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
           0,
           TargetDescriptorSet,
           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
       );
       RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.LogicalDevice, {}, { ImageWrite });
       CurrentLayerCount = RequestedPageCount;
   }
}

void RENDERER::ShadowMapManager::CreateShadowMapBuffers(
    bool MetaDataBufferReallocated,
    bool DataBufferReallocated,
    RENDERER_CORE::BufferAllocator &CurrentMetaDataBuffer,
    RENDERER_CORE::BufferAllocator &CurrentDataBuffer,
    VkDescriptorSet TargetDescriptorSet
)
{
    std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> BufferWrites;
    BufferWrites.reserve(2);
    if (MetaDataBufferReallocated)
    {
        RENDERER::RecreateBuffer(
            RendererContext,
            CurrentMetaDataBuffer.Allocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CurrentMetaDataBuffer.Buffer
        );
        RENDERER_CORE::DescriptorSetWriteBuffer MetaDataBufferWrite(
            CurrentMetaDataBuffer.Buffer,
            CurrentMetaDataBuffer.Allocator.GetCapacity(),
            1,
            TargetDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        BufferWrites.push_back(std::move(MetaDataBufferWrite));
    }

    if (DataBufferReallocated)
    {
        RENDERER::RecreateBuffer(
            RendererContext,
            CurrentDataBuffer.Allocator.GetCapacity(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            CurrentDataBuffer.Buffer
        );

        RENDERER_CORE::DescriptorSetWriteBuffer DataBufferWrite(
            CurrentDataBuffer.Buffer,
            CurrentDataBuffer.Allocator.GetCapacity(),
            2,
            TargetDescriptorSet,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        );
        BufferWrites.push_back(std::move(DataBufferWrite));
    }
    RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.LogicalDevice, BufferWrites, {});
}
