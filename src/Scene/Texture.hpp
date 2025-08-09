#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>

#include "SceneResource.hpp"
#include "../Renderer/Core/VulkanImage.hpp"

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	class Texture : public SceneResource
	{
	public:
		RENDERER_CORE::RawImageData ImageData;
	};
}