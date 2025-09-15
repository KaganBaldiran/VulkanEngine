#include "Material.hpp"

SCENE::Material::Material()
{
	std::fill(ReferencedTextures.begin(), ReferencedTextures.end(), TEXTURE_UNLINKED);
}

void SCENE::DoSpriteAnimation(glm::vec2& DestinationSize, glm::vec2& DestinationPosition, float& CurrentFrame, float DeltaTime, float Speed, uint32_t RowCount, uint32_t ColumnCount)
{
	uint32_t TotalFrameCount = RowCount * ColumnCount;
	CurrentFrame = (CurrentFrame + Speed * DeltaTime);
	if (CurrentFrame >= TotalFrameCount)
	{
		CurrentFrame = 0.0f;
	}
	uint32_t Xposition = static_cast<uint32_t>(CurrentFrame) % ColumnCount;
	uint32_t Yposition = static_cast<uint32_t>(CurrentFrame) / ColumnCount;

	DestinationSize = glm::vec2(1.0f / ColumnCount, 1.0f / RowCount);
	DestinationPosition = DestinationSize * glm::vec2(Xposition, Yposition);
}
