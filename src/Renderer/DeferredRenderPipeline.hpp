#pragma once
#include <array>

#include "RenderPipeline.hpp"
#include "GeometryBuffer.hpp"

#include "Core/VulkanSynchoronization.hpp"

namespace RENDERER
{
	class DeferredRenderPipeline : public RenderPipeline , COMMON::Destructible
	{
		friend class Renderer;
	public:
        DeferredRenderPipeline(RendererContext& RendererContext);
		DeferredRenderPipeline() = default;
		void Create(RendererContext& RendererContext) override;
	private:
		void RenderScene(
            SCENE::Scene& Scene, 
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            VkImageView& DepthImageImageView,
            VkImageView& DstRenderTargetImageView,
            bool EnableDepthTesting,
            bool ClearDepth,
            bool ClearColorAttachment
        ) override;
		void Destroy() override;
		void OnResize(uint32_t Width, uint32_t Height) override;

        void RenderGeometryPass(
            SCENE::Scene& Scene,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            VkImageView& DepthImage,
            bool EnableDepthTesting,
            bool ClearDepth
        );

        void RenderLightingPass(
            SCENE::Scene& Scene,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            VkImageView &DstRenderTargetImageView,
            bool ClearColorAttachment
        );

        //References
        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        uint32_t GraphicsQueueIndex;

        std::array<GeometryBuffer, MAX_FRAMES_IN_FLIGHT> Gbuffers;

        RENDERER_CORE::DescriptorPool DescriptorPool;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> LightingPassDescriptorSets;

        RENDERER_CORE::PipelineBarrier2 PipelineBarrier2;
		RendererContext* RendererContextPtr = nullptr;
	};
}