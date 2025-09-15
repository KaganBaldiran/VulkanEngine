#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

namespace RENDERER_CORE
{
	struct TextureData;

	class PipelineBarrier2
	{
	public:
		PipelineBarrier2();

		void AppendImageMemoryBarrier(
			VkImage Image,
			VkPipelineStageFlags2 SrcStageMask,
			VkPipelineStageFlags2 DstStageMask,
			VkAccessFlags2 SrcAccessMask,
			VkAccessFlags2 DstAccessMask,
			VkImageLayout OldLayout,
			VkImageLayout NewLayout,
			uint32_t SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			uint32_t DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			uint32_t BaseMipLevel = 0,
			uint32_t LevelCount = 1,
			uint32_t BaseArrayLayer = 0,
			uint32_t LayerCount = 1
		);
		void AppendBufferMemoryBarrier(
			VkBuffer Buffer,
			VkDeviceSize Offset,
			VkDeviceSize Size,
			VkPipelineStageFlags2 SrcStageMask,
			VkPipelineStageFlags2 DstStageMask,
			VkAccessFlags2 SrcAccessMask,
			VkAccessFlags2 DstAccessMask,
			uint32_t SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			uint32_t DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
		);
		void ExecutePipelineBarrier(VkCommandBuffer CommandBuffer);
	private:
		std::vector<VkImageMemoryBarrier2> ImageMemoryBarriers;
		std::vector<VkBufferMemoryBarrier2> BufferMemoryBarriers;
	};

	struct ImageBarrierState
	{
		VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags2 Stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		VkAccessFlags2 AccessMask = 0;
	};

	struct BufferBarrierState
	{
		VkPipelineStageFlags2 Stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		VkAccessFlags2 AccessMask = 0;
	};

	void SafeImageBarrier(
		VkImage& Image,
		ImageBarrierState& State,
		PipelineBarrier2& Barrier,
		VkImageLayout DestinationLayout,
		VkPipelineStageFlags2 DstStage,
		VkAccessFlags2 DstAccesMask,
		uint32_t SrcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		uint32_t DstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	);

	class TimelineSync
	{
	public:
		TimelineSync();
		~TimelineSync();

	private:

	};
}