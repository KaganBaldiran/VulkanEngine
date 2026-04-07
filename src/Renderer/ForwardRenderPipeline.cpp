#include "ForwardRenderPipeline.hpp"
#include "GeometryBuffer.hpp"

void RENDERER::ForwardRenderPipeline::Create(RendererContext& RendererContext)
{
}

void RENDERER::ForwardRenderPipeline::RenderScene(SCENE::Scene& Scene, SCENE::Camera3D& Camera, VkCommandBuffer& CommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame, VkImageView& DepthImageImageView, VkImageView& DstColorRenderTargetImageViews, GeometryBuffer& FrameGbuffer, VkDescriptorSet& GeometrybufferDescriptorSet, bool EnableDepthTesting, bool ClearDepth, bool ClearColorAttachment)
{
}

void RENDERER::ForwardRenderPipeline::Destroy()
{
}

void RENDERER::ForwardRenderPipeline::OnResize(uint32_t Width, uint32_t Height)
{
}
