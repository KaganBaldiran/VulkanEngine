#include "ShadowMapManager.hpp"
#include "../Scene/Light.hpp"

#include <array>
#include <string>

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Renderer/MeshManager.hpp"

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
    /*
    ShadowMapTexture.Destroy(RendererContext->DeviceContext.logicalDevice);
    CascadedShadowMapsMetaDataBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
    */
}

void RENDERER::ShadowMapManager::AppendCascadedShadowMap(CascadedShadowMapAppendInfo Info)
{
    auto& InputInfos = Info.Infos;
    const auto& FrameIndex = Info.FrameIndex;

    auto& CurrentMetaDataBuffer = CascadedShadowMapsMetaDataBuffers[FrameIndex];
    auto& CurrentDataBuffer = CascadedShadowMapsDataBuffers[FrameIndex];
    auto& CurrentMetaDataBufferAllocator = CurrentMetaDataBuffer.Allocator;
    auto& CurrentDataBufferAllocator = CurrentDataBuffer.Allocator;

    TexturePacker3D& CurrentTexturePacker = TexturePackers[FrameIndex];
    size_t InitialPageCount = CurrentTexturePacker.GetPageCount();

    auto& CurrentShadowMapEntries = this->CascadedShadowMapEntries[FrameIndex];
    size_t SizeOfMetaData = sizeof(CascadedMapMetaData), SizeOfData = sizeof(CascadedMapData);
    size_t InitialCascadeDataBufferCapacity = CurrentTexturePacker.GetPageCount();
    size_t InitialPageCount = CurrentTexturePacker.GetPageCount();

    for (auto& InputInfo : InputInfos)
    {
        if (!InputInfo.SourceLight) continue;

        auto Iterator = CurrentShadowMapEntries.find(InputInfo.SourceLight->GetHandleID());
        if (Iterator != CurrentShadowMapEntries.end()) continue;

        RENDERER_CORE::MemoryRegion DataBatchRegion = CurrentDataBufferAllocator.Suballocate(SizeOfData * InputInfo.CascadeCount);

        CascadedMapMetaData MetaData{};
        MetaData.CascadeCount = InputInfo.CascadeCount;
        MetaData.Offset = DataBatchRegion.Offset / SizeOfMetaData;

        CascadedShadowMapEntry NewShadowMapEntry{};
        NewShadowMapEntry.MetaDataMemoryRegion = CurrentMetaDataBufferAllocator.Suballocate(SizeOfMetaData);
        NewShadowMapEntry.MetaData = MetaData;
        NewShadowMapEntry.CascadeEntries.reserve(InputInfo.CascadeCount);
        for (size_t i = 0; i < InputInfo.CascadeCount; i++)
        {
            MemoryRegion3D NewTextureRegion = CurrentTexturePacker.Insert({ 1024,1024 });

            CascadedMapData NewCascadeData{};
            NewCascadeData.TextureSize = NewTextureRegion.Region.Size.x;
            NewCascadeData.TexturePosition = NewTextureRegion.Region.Offset;
            NewCascadeData.TextureLayer = NewTextureRegion.Layer;

            CascadeEntry NewEntry{};
            NewEntry.TextureRegion = NewTextureRegion;
            NewEntry.MemoryRegion = { DataBatchRegion.Offset + i * SizeOfData,SizeOfData };
            NewEntry.Data = NewCascadeData;
            NewShadowMapEntry.CascadeEntries.push_back(NewEntry);
        }
    }
    
    if()
}

void RENDERER::ShadowMapManager::CreateShadowMapTextures(size_t RequestedPageCount, size_t FrameIndex)
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
           CurrentLayerCount
       );

       for (size_t i = 0; i < CurrentLayerCount; i++)
       {
           RENDERER_CORE::CreateImageView(
               CurrentShadowMapTextures.Image,
               ShadowMapImageFormat,
               VK_IMAGE_VIEW_TYPE_2D,
               VK_IMAGE_ASPECT_DEPTH_BIT,
               RendererContext->DeviceContext.LogicalDevice
           );
       }

       RENDERER_CORE::CreateTextureSampler(
           RendererContext->DeviceContext.PhysicalDevice,
           RendererContext->DeviceContext.LogicalDevice,
           CurrentShadowMapTextures.Samplers[0],
           VK_FILTER_LINEAR,
           VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
       );
       CurrentLayerCount = RequestedPageCount;
   }
}

void RENDERER::ShadowMapManager::CreateShadowMapBuffers()
{
    /*
    RENDERER::RecreateBuffer(
        RendererContext,
        ModelMatricesBuffer.Allocator.GetCapacity(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        ModelMatricesBuffer.Buffer.Buffer
    );
    */
}
