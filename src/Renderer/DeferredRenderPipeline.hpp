#pragma once
#include <array>

#include "RenderPipeline.hpp"
#include "GeometryBuffer.hpp"

#include "Core/VulkanSynchoronization.hpp"
#include <chrono>

namespace RENDERER
{
	class DeferredRenderPipeline : public RenderPipeline , COMMON::Destructible
	{
		friend class Renderer;
	public:
        DeferredRenderPipeline(RendererContext& RendererContext);
		DeferredRenderPipeline() = default;
		void Create(RendererContext& RendererContext) override;

        /// Allows creating the pipeline with custom parameters
        /// <param name="ShadePixelFunction">
        ///  Custom pixel shading function
        ///  vec3 ShadePixel(in vec3 CameraPosition,in vec3 CameraDirection,in vec3 Normal, in vec3 Position, in vec3 Albedo, in float Roughness, in float Metallic,in float Time)
        ///  {
        /// 
        ///     return vec3();
        ///  }
        ///  In addition to default GLSL functions also the following functions are defined and can be used
        ///  vec3 CalculateLighting(in vec3 Normal,in vec3 Position,in vec3 Albedo,in float Roughness,in float Metallic) Outputs PBR shading
        ///  float FresnelSchlick(float cosTheta , float F0)
        ///  vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
        ///  float DistributionGGX(vec3 N , vec3 H, float roughness)
        /// </param>
        void CompileCustomPipeline(std::string ShadePixelFunction, const char* Label);
	private:
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

        void RenderGeometryPass(
            SCENE::Scene& Scene,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            VkImageView& DepthImage,
            GeometryBuffer& Gbuffers,
            bool EnableDepthTesting,
            bool ClearDepth
        );

        void RenderLightingPass(
            SCENE::Scene& Scene,
            VkCommandBuffer& CommandBuffer,
            uint32_t CurrentImageIndex,
            uint32_t CurrentFrame,
            VkImageView &DstRenderTargetImageView,
            VkDescriptorSet& GeometrybufferDescriptorSet,
            bool ClearColorAttachment
        );
        glm::mat4 PreviousProjViewMatrix;
        RENDERER_CORE::GraphicsPipeline Pipeline;

        std::chrono::system_clock::time_point StartingTime;

        //References
        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;
        uint32_t GraphicsQueueIndex;

        RENDERER_CORE::PipelineBarrier2 PipelineBarrier2;
		RendererContext* RendererContextPtr = nullptr;
	};
}