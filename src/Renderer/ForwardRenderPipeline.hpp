#pragma once
#include "RenderPipeline.hpp"
#include "Core/VulkanSynchoronization.hpp"

#include <chrono>
#include <array>

namespace RENDERER
{
	//Forward declarations 
	class Renderer;
	class GeometryBuffer;

	class ForwardRenderPipeline : public RenderPipeline, COMMON::Destructible
	{
		friend class Renderer;
	public:
		ForwardRenderPipeline() = default;

		void Create(RendererContext& RendererContext) override;
	protected:
		void RenderScene(
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
		) override;
		void Destroy() override;
		void OnResize(uint32_t Width, uint32_t Height) override;

		glm::mat4 PreviousProjViewMatrix;
		RENDERER_CORE::GraphicsPipeline Pipeline;
		std::chrono::system_clock::time_point StartingTime;

		void RenderLightingPass(
			SCENE::Scene& Scene,
			VkCommandBuffer& CommandBuffer,
			uint32_t CurrentImageIndex,
			uint32_t CurrentFrame,
			VkImageView& DepthImage,
			GeometryBuffer& Gbuffers,
			bool EnableDepthTesting,
			bool ClearDepth
		);

		//References
		uint32_t GraphicsQueueIndex;

		RENDERER_CORE::PipelineBarrier2 PipelineBarrier2;
		RendererContext* RendererContextPtr = nullptr;
	};
}