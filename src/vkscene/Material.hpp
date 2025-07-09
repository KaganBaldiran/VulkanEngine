#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

#include "SceneResource.hpp"

namespace VKSCENE
{
	static constexpr uint64_t TEXTURE_UNLINKED = 0;

	enum MaterialTextureType
	{
		MATERIAL_TEXTURE_TYPE_ALBEDO = 0,
		MATERIAL_TEXTURE_TYPE_ROUGHNESS = 1,
		MATERIAL_TEXTURE_TYPE_NORMAL_MAP = 2,
		MATERIAL_TEXTURE_TYPE_METALLIC = 3,
		MATERIAL_TEXTURE_TYPE_OPACITY = 4
	};

	class Material : public SceneResource
	{
	public:
		Material();

		glm::vec4 Albedo;
		float Roughness;
		float Metallic;
		float Opacity;

		std::array<uint64_t, 5> ReferencedTextures;
		inline void ReferenceTexture(const uint64_t& TextureID, MaterialTextureType TextureTypeSlot) { ReferencedTextures[static_cast<size_t>(TextureTypeSlot)] = TextureID; };
		inline uint64_t GetTexture(MaterialTextureType TextureType) { return ReferencedTextures[static_cast<size_t>(TextureType)]; };
	private:
	};
}
