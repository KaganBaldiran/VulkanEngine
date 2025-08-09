#include "ShadowMapManager.hpp"
#include "Light.hpp"

#include <array>
#include <string>

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

std::array<glm::vec3, 8> SCENE::GetCameraFrustum(glm::mat4 InverseProjectMatrix, glm::mat4 InverseViewMatrix)
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

glm::mat4 SCENE::GetLightSpaceMatrix(glm::vec3 LightDirection,std::array<glm::vec3, 8>& Frustum)
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

void SCENE::ShadowMapManager::Destroy(VkDevice LogicalDevice)
{
}

SCENE::TexturePacker3D::TexturePacker3D(glm::ivec2 PageSize)
{
    Create(PageSize);
}

void SCENE::TexturePacker3D::Create(glm::ivec2 PageSize)
{
    this->PageSize = PageSize;
    Pages.reserve(100);

    TextureLayer Layer;
    Layer.FreePixelCount = static_cast<size_t>(PageSize.x) * PageSize.y;
    Pages.push_back(Layer);
}

SCENE::MemoryRegion3D SCENE::TexturePacker3D::Insert(glm::ivec2 Size)
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

void SCENE::TexturePacker3D::Erase(const MemoryRegion3D& Region)
{
    if (Region.Layer >= Pages.size()) return;
    auto& Page = Pages[Region.Layer];
    auto Iterator = Page.Regions.find(Region.Region);
    if (Iterator == Page.Regions.end())
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_ERROR, std::string("Attempting to erase an nonexistent 3D memory region [offset(" 
                                        + std::to_string(Region.Region.Offset.x) + "," + std::to_string(Region.Region.Offset.y) + 
                                        ")size(" + std::to_string(Region.Region.Size.x) + "," + std::to_string(Region.Region.Size.y) + ")]."));
        throw std::runtime_error("Attempting to erase an nonexistent 3D memory region.");
    }
    Page.FreePixelCount += Region.Region.Size.x * Region.Region.Size.y;
    Page.Regions.erase(Region.Region);
}

const SCENE::MemoryRegion2D* SCENE::TextureLayer::DoesOverlap(
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

size_t SCENE::MemoryRegion2DHasher::operator()(const MemoryRegion2D& Region) const
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