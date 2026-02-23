#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Renderer/Core/VulkanImage.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Scene/PersistentSceneStagingBuffer.hpp"

#include <unordered_set>
#include <array>
#include <map>

namespace SCENE
{
	class Light;
}

namespace RENDERER
{
	//Forward Declarations 
	class RendererContext;
	struct CopyOperationEntry;

	std::array<glm::vec3, 8> GetCameraFrustum(glm::mat4 InverseProjectMatrix, glm::mat4 InverseViewMatrix);
	glm::mat4 GetLightSpaceMatrix(glm::vec3 LightDirection, std::array<glm::vec3, 8>& Frustum);

	struct MemoryRegion2D
	{
		glm::ivec2 Size = glm::ivec2(0, 0);
		glm::ivec2 Offset = glm::ivec2(0, 0);

		inline bool operator==(const MemoryRegion2D& Other) const
		{
			return Size == Other.Size && Offset == Other.Offset;
		}
	};

	struct MemoryRegion3D
	{
		MemoryRegion2D Region;
		uint32_t Layer = std::numeric_limits<uint32_t>::max();

		inline bool operator==(const MemoryRegion3D& Other) const
		{
			return Region == Other.Region && Layer == Other.Layer;
		}
	};

	struct MemoryRegion2DHasher
	{
		size_t operator()(const MemoryRegion2D& Region) const;
	};

	struct TextureLayer
	{
		size_t FreePixelCount;
		//std::vector<uint8_t> LayerBitmap;
		std::unordered_set<MemoryRegion2D, MemoryRegion2DHasher> Regions;
		static const MemoryRegion2D* DoesOverlap(
			const std::vector<const MemoryRegion2D*>& Regions,
			float Min_x,
			float Min_y,
			float Max_x,
			float Max_y
		);
	};

	//Virtually packs 2D textures with various sizes into a large layered 2D texture
	class TexturePacker3D
	{
	public:
		TexturePacker3D(glm::ivec2 PageSize);
		TexturePacker3D() = default;
		void Create(glm::ivec2 PageSize);

		MemoryRegion3D Insert(glm::ivec2 Size);
		void Erase(const MemoryRegion3D& Region);
		size_t GetPageCount() const { return Pages.size(); };
	private:
		std::vector<TextureLayer> Pages;
		glm::ivec2 PageSize;
	};

	//Meta data needed for cascaded shadow maps
	struct CascadedMapData
	{
		glm::vec2 TexturePosition;
		float TextureSize;
		float TextureLayer;
		float Distance;
		float ShadowCascadeLevel;
		glm::mat4 LightMatrix;
	};

	struct CascadedMapMetaData
	{
		float CascadeCount;
		float Offset;
		glm::vec4 LightDirection;
	};

	struct CascadeEntry
	{
		CascadedMapData Data;
		RENDERER_CORE::MemoryRegion MemoryRegion;
		RENDERER_CORE::MemoryRegion StagingMemoryRegion;
		MemoryRegion3D TextureRegion;
	};

	struct CascadedShadowMapEntry
	{
		CascadedMapMetaData MetaData;
		RENDERER_CORE::MemoryRegion MetaDataMemoryRegion;
		RENDERER_CORE::MemoryRegion StagingMetaDataMemoryRegion;
		std::vector<CascadeEntry> CascadeEntries;
		bool RequiresUpload = true;
	};

	enum CascadedShadowMapDistanceFunction
	{
		CASCADED_SHADOW_MAP_DISTANCE_FUNCTION_LINEAR = 1,
		CASCADED_SHADOW_MAP_DISTANCE_FUNCTION_LOGARITHMIC = 2
	};

	struct ShadowMapCascade
	{
		glm::ivec2 TextureSize = { 1024,1024 };
		float Distance;
	};

	struct CascadedShadowMapInfo
	{
		SCENE::Light* SourceLight = nullptr;
		std::vector<ShadowMapCascade> Cascades;
		CascadedShadowMapDistanceFunction DistanceFunction = CASCADED_SHADOW_MAP_DISTANCE_FUNCTION_LOGARITHMIC;
	};

	//Centralized manager responsible for storing and managing shadow maps 
	class ShadowMapManager
	{
	public:
		ShadowMapManager(RENDERER::RendererContext& RendererContext, glm::ivec2 PageSize);
		ShadowMapManager() = default;
		void Create(RENDERER::RendererContext& RendererContext,glm::ivec2 PageSize);
		void Destroy();

		void AppendCascadedShadowMap(
			std::vector<CascadedShadowMapInfo>& Infos,
			uint32_t FrameIndex,
			std::array<CopyOperationEntry*, 2>& CopyInfos,
			std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& TargetDescriptorSets,
			SCENE::PersistentStagingBuffer& StagingBuffer
		);
		void RenderShadowMaps();
	private:
		void CreateShadowMapTextures(
			size_t RequestedPageCount,
			size_t FrameIndex, 
			VkDescriptorSet TargetDescriptorSet
		);
		void CreateShadowMapBuffers(
			bool MetaDataBufferReallocated,
			bool DataBufferReallocated,
			RENDERER_CORE::BufferAllocator& CurrentMetaDataBuffer,
			RENDERER_CORE::BufferAllocator& CurrentDataBuffer,
			VkDescriptorSet TargetDescriptorSet
		);

		std::array<RENDERER_CORE::TextureDataMultipleSamplerViews,MAX_FRAMES_IN_FLIGHT> ShadowMapTextures;
		std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT> CascadedShadowMapsMetaDataBuffers;
		std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT> CascadedShadowMapsDataBuffers;
		glm::ivec2 PageSize;
		std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> LayerCount;
		std::array<std::map<size_t, CascadedShadowMapEntry>,MAX_FRAMES_IN_FLIGHT> CascadedShadowMapEntries;
		std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> CopyInfoIndices;

		std::array<TexturePacker3D, MAX_FRAMES_IN_FLIGHT> TexturePackers;
		VkFormat ShadowMapImageFormat;
		RENDERER::RendererContext* RendererContext;
	};
}
