#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>

#include "SceneResource.hpp"
#include "../vkcore/VulkanImage.hpp"

namespace VKAPP
{
	class RendererContext;
}

namespace VKSCENE
{
	class Texture : public SceneResource
	{
	public:
		VKCORE::RawImageData ImageData;
	};
}