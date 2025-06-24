#include "Cubemap.hpp"
#include "../app/RendererContext.hpp"
#include "../include/stbi/stb_image.h"
#include "../vkcore/VulkanPipeline.hpp"

VKSCENE::Cubemap::Cubemap(VKAPP::RendererContext& RendererContext,uint32_t Width, uint32_t Height)
{
	Create(RendererContext,Width, Height);
}

void VKSCENE::Cubemap::Create(VKAPP::RendererContext& RendererContext, uint32_t Width, uint32_t Height)
{
	Size = { Width,Height };
	VKCORE::CreateImage(
		RendererContext.DeviceContext.physicalDevice, 
		RendererContext.DeviceContext.logicalDevice,
		Width, 
		Height, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
	    Image, 
		ImageMemory,
		6,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	);

	for (uint32_t i = 0; i < 6; i++)
	{
		RenderImageViews[i] = VKCORE::CreateImageView(
			Image,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_COLOR_BIT,
			RendererContext.DeviceContext.logicalDevice,
			6,
			i
		);
	}

	SampleImageView = VKCORE::CreateImageView(
		Image,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_VIEW_TYPE_CUBE,
		VK_IMAGE_ASPECT_COLOR_BIT,
		RendererContext.DeviceContext.logicalDevice,
		6,
		0
	);
	
	VKCORE::CreateTextureSampler(
		RendererContext.DeviceContext.physicalDevice,
		RendererContext.DeviceContext.logicalDevice,
		Sampler, 
		VK_FILTER_LINEAR, 
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
	);
}

void VKSCENE::Cubemap::Destroy(VKAPP::RendererContext& RendererContext)
{
	for (auto &ImageView : RenderImageViews)
	{
		if (ImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(RendererContext.DeviceContext.logicalDevice, ImageView, nullptr);
			ImageView = VK_NULL_HANDLE;
		}
	}
	if (SampleImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(RendererContext.DeviceContext.logicalDevice, SampleImageView, nullptr);
		SampleImageView = VK_NULL_HANDLE;
	}
	if (Sampler != VK_NULL_HANDLE) {
		vkDestroySampler(RendererContext.DeviceContext.logicalDevice, Sampler, nullptr);
		Sampler = VK_NULL_HANDLE;
	}
	if (Image != VK_NULL_HANDLE) {
		vkDestroyImage(RendererContext.DeviceContext.logicalDevice, Image, nullptr);
		Image = VK_NULL_HANDLE;
	}
	if (ImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(RendererContext.DeviceContext.logicalDevice, ImageMemory, nullptr);
		ImageMemory = VK_NULL_HANDLE;
	}
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

	auto& RenderTask = [&](VkCommandBuffer& CommandBuffer) {

		VKCORE::TransitionImageLayout(CommandBuffer,HdriTextureData.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,6);

		VkClearValue ClearColor{};
		ClearColor.color = { {0.0f,1.0f,0.0f,0.0f} };

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

		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererContext.HDRIrenderGraphicsPipeline.Layout, 0, 1, &RendererContext.HDRIrenderPassDescriptorSets[0], 0, nullptr);

		glm::mat4 Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		Projection[1][1] *= -1;
		for (size_t i = 0; i < 6; i++)
		{
			VKCORE::DynamicRenderingPass RenderingPass;
			RenderingPass.AppendAttachment(
				DestinationCubeMap.RenderImageViews[i],
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

		VKCORE::TransitionImageLayout(CommandBuffer, DestinationCubeMap.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT,6);
	};

	VKCORE::ExecuteSingleTimeCommand(
		RendererContext.DeviceContext.logicalDevice,
		RenderTask,
		RendererContext.CommandPool.commandPool,
		RendererContext.DeviceContext.GraphicsQueue
	);

	HdriTextureData.Destroy(RendererContext.DeviceContext.logicalDevice);
}
