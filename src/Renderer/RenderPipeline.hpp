#pragma once
#include <iostream>
#include "Core/VulkanCommandBuffer.hpp"
#include "../Common/Handle.hpp"

namespace SCENE
{
	class Scene;
}

namespace RENDERER
{
	class Renderer;
	class RendererContext;
	class GeometryBuffer;

	enum RenderPipelineType
	{
		RENDER_PIPELINE_TYPE_UNSPECIFIED = 0,
		RENDER_PIPELINE_TYPE_DEFERRED_RENDER = 1,
		RENDER_PIPELINE_TYPE_FORWARD_RENDER = 2
	};

	class RenderPipeline : public COMMON::Handle
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
			VkImageView& DstColorRenderTargetImageViews,
			GeometryBuffer& FrameGbuffer,
			VkDescriptorSet& GeometrybufferDescriptorSet,
			bool EnableDepthTesting,
			bool ClearDepth,
			bool ClearColorAttachment
		) = 0;
		virtual void Destroy() = 0;
		virtual void OnResize(uint32_t Width, uint32_t Height) = 0;

		RenderPipelineType PipelineType = RENDER_PIPELINE_TYPE_UNSPECIFIED;
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
