#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

namespace VKCORE
{
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
}