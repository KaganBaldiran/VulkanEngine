#pragma once
#include <iostream>
#include "RendererContext.hpp"

namespace SCENE
{
	class Scene;
}

namespace RENDERER
{
	class Renderer;

	class RenderPipeline
	{
		friend class Renderer;
	public:
		RenderPipeline() = default;
		
		virtual void Create(RendererContext &RendererContext) = 0;
	protected:
		virtual void RenderScene(
			SCENE::Scene& Scene,
			VkCommandBuffer& CommandBuffer,
			uint32_t CurrentImageIndex,
			uint32_t CurrentFrame,
			VkImageView& DepthImageImageView,
			VkImageView& DstRenderTargetImageView,
			bool EnableDepthTesting,
			bool ClearDepth,
			bool ClearColorAttachment
		) = 0;
		virtual void Destroy() = 0;
		virtual void OnResize(uint32_t Width, uint32_t Height) = 0;

		RendererContext* RendererContextPtr = nullptr;
	};

	struct RenderPassConfiguration
	{
		std::string Name;
		RenderPipeline* Pipeline;
		bool EnableDepthTesting;
		SCENE::Scene* Scene;
	};
}
