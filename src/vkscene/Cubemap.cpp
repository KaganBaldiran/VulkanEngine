#include "Cubemap.hpp"
#include "../app/RendererContext.hpp"
#include "../include/stbi/stb_image.h"
#include "../vkcore/VulkanPipeline.hpp"

#include "DependencyManager.hpp"

VKSCENE::Cubemap::Cubemap(VKAPP::RendererContext& RendererContext, ResourceDependencyManager& DependencyManager,uint32_t Width, uint32_t Height)
{
	Create(RendererContext,DependencyManager,Width, Height);
}

void VKSCENE::Cubemap::Create(VKAPP::RendererContext& RendererContext, ResourceDependencyManager& DependencyManager, uint32_t Width, uint32_t Height)
{
	Size = { Width,Height };
	
	CreateCubemapTexture(RendererContext);
	CreateConvolutionTexture(RendererContext);

	this->dependencyManager = &DependencyManager;
	resourceType = RESOURCE_TYPE_CUBEMAP;
}

void VKSCENE::Cubemap::Destroy(VKAPP::RendererContext& RendererContext)
{
	for (auto &ImageView : CubemapRenderImageViews)
	{
		if (ImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(RendererContext.DeviceContext.logicalDevice, ImageView, nullptr);
			ImageView = VK_NULL_HANDLE;
		}
	}
	if (CubemapSampleImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(RendererContext.DeviceContext.logicalDevice, CubemapSampleImageView, nullptr);
		CubemapSampleImageView = VK_NULL_HANDLE;
	}
	if (CubemapSampler != VK_NULL_HANDLE) {
		vkDestroySampler(RendererContext.DeviceContext.logicalDevice, CubemapSampler, nullptr);
		CubemapSampler = VK_NULL_HANDLE;
	}
	if (CubemapImage != VK_NULL_HANDLE) {
		vkDestroyImage(RendererContext.DeviceContext.logicalDevice, CubemapImage, nullptr);
		CubemapImage = VK_NULL_HANDLE;
	}
	if (CubemapImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(RendererContext.DeviceContext.logicalDevice, CubemapImageMemory, nullptr);
		CubemapImageMemory = VK_NULL_HANDLE;
	}
}

void VKSCENE::Cubemap::CreateCubemapTexture(VKAPP::RendererContext& RendererContext)
{
	VKCORE::CreateImage(
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		Size.x,
		Size.y,
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		CubemapImage,
		CubemapImageMemory,
		6,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	);

	for (uint32_t i = 0; i < 6; i++)
	{
		CubemapRenderImageViews[i] = VKCORE::CreateImageView(
			CubemapImage,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT,
			RendererContext.DeviceContext.logicalDevice,
			6,
			i
		);
	}

	CubemapSampleImageView = VKCORE::CreateImageView(
		CubemapImage,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_VIEW_TYPE_CUBE,
		VK_IMAGE_ASPECT_COLOR_BIT,
		RendererContext.DeviceContext.logicalDevice,
		6,
		0
	);

	VKCORE::CreateTextureSampler(
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		CubemapSampler,
		VK_FILTER_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
	);
}

void VKSCENE::Cubemap::CreateConvolutionTexture(VKAPP::RendererContext& RendererContext)
{
	VKCORE::CreateImage(
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		32,
		32,
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		ConvolutionImage,
		ConvolutionImageMemory,
		6,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	);

	for (uint32_t i = 0; i < 6; i++)
	{
		ConvolutionRenderImageViews[i] = VKCORE::CreateImageView(
			ConvolutionImage,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT,
			RendererContext.DeviceContext.logicalDevice,
			6,
			i
		);
	}

	ConvolutionSampleImageView = VKCORE::CreateImageView(
		ConvolutionImage,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_VIEW_TYPE_CUBE,
		VK_IMAGE_ASPECT_COLOR_BIT,
		RendererContext.DeviceContext.logicalDevice,
		6,
		0
	);

	VKCORE::CreateTextureSampler(
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		ConvolutionSampler,
		VK_FILTER_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
	);
}

void VKSCENE::ImportHDRI(const char* HDRIfilePath, Cubemap& DestinationCubeMap, VKAPP::RendererContext& RendererContext)
{
	VKCORE::TextureData HdriTextureData;
	VKCORE::CreateTextureImage(
		HDRIfilePath,
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		RendererContext.CommandPool.commandPool,
		RendererContext.DeviceContext.GraphicsQueue,
		HdriTextureData
	);

	VKCORE::DescriptorSetWriteImage HDRITextureWrite(HdriTextureData.ImageView, HdriTextureData.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, RendererContext.HDRIrenderPassDescriptorSets[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	VKCORE::WriteDescriptorSets(RendererContext.DeviceContext.logicalDevice, {}, {HDRITextureWrite});
	
	glm::mat4 FboViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	glm::mat4 Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	Projection[1][1] *= -1;

	auto& RenderTask = [&](VkCommandBuffer& CommandBuffer) {

		VKCORE::TransitionImageLayout(CommandBuffer,HdriTextureData.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.CubemapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,6);

		VkClearValue ClearColor{};
		ClearColor.color = { {0.0f,0.0f,0.0f,0.0f} };

		vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContext.HDRIrenderGraphicsPipeline.pipeline);

		VkViewport Viewport{};
		Viewport.x = 0.0f;
		Viewport.y = 0.0f;
		Viewport.width = static_cast<float>(DestinationCubeMap.Size.x);
		Viewport.height = static_cast<float>(DestinationCubeMap.Size.y);
		Viewport.minDepth = 0.0f;
		Viewport.maxDepth = 1.0f;
		vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

		VkExtent2D ScissorExtend{};
		ScissorExtend.width = (uint32_t)DestinationCubeMap.Size.x;
		ScissorExtend.height = (uint32_t)DestinationCubeMap.Size.y;

		VkRect2D Scissor{};
		Scissor.offset = { 0,0 };
		Scissor.extent = ScissorExtend;
		vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

		VkBuffer VertexBuffers[] = { RendererContext.CubeVertexBuffer.BufferObject };
		VkDeviceSize Offsets[] = { 0 };
		vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, Offsets);

		//HDRI to cubemap pass
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContext.HDRIrenderGraphicsPipeline.Layout, 0, 1, &RendererContext.HDRIrenderPassDescriptorSets[0], 0, nullptr);

		for (size_t i = 0; i < 6; i++)
		{
			VKCORE::DynamicRenderingPass RenderingPass;
			RenderingPass.AppendAttachment(
				DestinationCubeMap.CubemapRenderImageViews[i],
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE,
				ClearColor
			);
			RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {(uint32_t)DestinationCubeMap.Size.x, (uint32_t)DestinationCubeMap.Size.y} });

			std::array<glm::mat4, 2> Matrixes = { FboViews[i],Projection };
			vkCmdPushConstants(
				CommandBuffer,
				RendererContext.HDRIrenderGraphicsPipeline.Layout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				2 * sizeof(glm::mat4), Matrixes.data()
			);

			vkCmdDraw(CommandBuffer, 36, 1, 0, 0);

			RenderingPass.EndRendering(CommandBuffer);
		}

		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.CubemapImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT,6);
		    	
		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.ConvolutionImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 6);

		//Convolution Pass
		VKCORE::DescriptorSetWriteImage CubemapTextureWrite(DestinationCubeMap.CubemapSampleImageView, DestinationCubeMap.CubemapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, RendererContext.HDRIrenderPassDescriptorSets[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VKCORE::WriteDescriptorSets(RendererContext.DeviceContext.logicalDevice, {}, { CubemapTextureWrite });

		vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContext.HDRIconvoluteGraphicsPipeline.pipeline);
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContext.HDRIconvoluteGraphicsPipeline.Layout, 0, 1, &RendererContext.HDRIrenderPassDescriptorSets[0], 0, nullptr);

		Viewport.x = 0.0f;
		Viewport.y = 0.0f;
		Viewport.width = 32.0f;
		Viewport.height = 32.0f;
		Viewport.minDepth = 0.0f;
		Viewport.maxDepth = 1.0f;
		vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

		ScissorExtend.width = 32;
		ScissorExtend.height = 32;

		Scissor.offset = { 0,0 };
		Scissor.extent = ScissorExtend;
		vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

		for (size_t i = 0; i < 6; i++)
		{
			VKCORE::DynamicRenderingPass RenderingPass;
			RenderingPass.AppendAttachment(
				DestinationCubeMap.ConvolutionRenderImageViews[i],
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE,
				ClearColor
			);
			RenderingPass.BeginRendering(CommandBuffer, VkRect2D{ {0, 0}, {32, 32} });

			std::array<glm::mat4, 2> Matrixes = { FboViews[i],Projection };
			vkCmdPushConstants(
				CommandBuffer,
				RendererContext.HDRIconvoluteGraphicsPipeline.Layout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				2 * sizeof(glm::mat4), Matrixes.data()
			);

			vkCmdDraw(CommandBuffer, 36, 1, 0, 0);

			RenderingPass.EndRendering(CommandBuffer);
		}

		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.ConvolutionImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 6);

	};

	VKCORE::ExecuteSingleTimeCommand(
		RendererContext.DeviceContext.logicalDevice,
		RenderTask,
		RendererContext.CommandPool.commandPool,
		RendererContext.DeviceContext.GraphicsQueue
	);

	HdriTextureData.Destroy(RendererContext.DeviceContext.logicalDevice);
}
