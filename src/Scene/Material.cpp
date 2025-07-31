#include "Material.hpp"

VKSCENE::Material::Material()
{
	std::fill(ReferencedTextures.begin(), ReferencedTextures.end(), TEXTURE_UNLINKED);
}

