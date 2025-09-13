#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

#include "SceneResource.hpp"

namespace SCENE
{
	static constexpr uint64_t TEXTURE_UNLINKED = 0;

	enum MaterialTextureType
	{
		MATERIAL_TEXTURE_TYPE_ALBEDO = 0,
		MATERIAL_TEXTURE_TYPE_ROUGHNESS = 1,
		MATERIAL_TEXTURE_TYPE_NORMAL_MAP = 2,
		MATERIAL_TEXTURE_TYPE_METALLIC = 3,
		MATERIAL_TEXTURE_TYPE_OPACITY = 4,
		MATERIAL_TEXTURE_TYPE_META_DATA_SIZE
	};

	class Material
	{
	public:
		Material();
		
		glm::vec4 Albedo = glm::vec4(1.0f);
		float Roughness = 0.5f;
		float Metallic = 0.0f;
		float Opacity = 1.0f;

		glm::vec2 TextureSamplePosition = glm::vec2(0.0f);
		glm::vec2 TextureSampleSize = glm::vec2(1.0f);

		std::array<uint64_t, 5> ReferencedTextures;
		inline void ReferenceTexture(const uint64_t& TextureID, MaterialTextureType TextureTypeSlot) { ReferencedTextures[static_cast<size_t>(TextureTypeSlot)] = TextureID; };
		inline uint64_t GetTexture(MaterialTextureType TextureType) { return ReferencedTextures[static_cast<size_t>(TextureType)]; };
	};
}
