#include "Material.hpp"

SCENE::Material::Material()
{
	std::fill(ReferencedTextures.begin(), ReferencedTextures.end(), TEXTURE_UNLINKED);
}

