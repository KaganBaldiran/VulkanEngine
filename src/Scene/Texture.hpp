#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../Common/Handle.hpp"

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	class Texture : public COMMON::Handle
	{
	public:
		//RENDERER_CORE::RawImageData ImageData;
	};
}