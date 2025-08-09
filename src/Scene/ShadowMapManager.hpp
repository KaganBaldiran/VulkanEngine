#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Renderer/Core/VulkanBuffer.hpp"

#include <unordered_set>
#include <array>
#include <map>

namespace SCENE
{
	//Forward Declarations 
	class Light;
	std::array<glm::vec3, 8> GetCameraFrustum(glm::mat4 InverseProjectMatrix, glm::mat4 InverseViewMatrix);
	glm::mat4 GetLightSpaceMatrix(glm::vec3 LightDirection, std::array<glm::vec3, 8>& Frustum);

	struct MemoryRegion2D
	{
		glm::ivec2 Size = glm::ivec2(0, 0);
		glm::ivec2 Offset = glm::ivec2(0,0);

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

	class TexturePacker3D
	{
	public:
		TexturePacker3D(glm::ivec2 PageSize);
		TexturePacker3D() = default;
		void Create(glm::ivec2 PageSize);

		MemoryRegion3D Insert(glm::ivec2 Size);
		void Erase(const MemoryRegion3D& Region);
	private:
		std::vector<TextureLayer> Pages;
		glm::ivec2 PageSize;
	};

	struct CascadedShadowMapInfo
	{
		Light* SourceLight;

	};

	struct CascadedShadowMapEntry
	{

	};

	class ShadowMapManager
	{
	public:

		ShadowMapManager() = default;
		void Create();
		void Destroy(VkDevice LogicalDevice);


	private:
		RENDERER_CORE::Buffer CascadedShadowMapsBuffer;
		std::map<Light*, CascadedShadowMapEntry,std::less<Light*>> CascadedShadowMapEntries;
	};
}
