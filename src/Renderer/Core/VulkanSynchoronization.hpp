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

	struct MemoryBufferBarrierState
	{
		VkPipelineStageFlags2 SrcStageMask;
		VkPipelineStageFlags2 DstStageMask;
		VkAccessFlags2 SrcAccessMask;
		VkAccessFlags2 DstAccessMask;
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

	class Fence
	{
	public:
		Fence(VkDevice LogicalDevice, VkFenceCreateFlags Flags);
		Fence() = default;
		void Create(VkDevice LogicalDevice, VkFenceCreateFlags Flags);
		void Destroy(VkDevice LogicalDevice);
		VkFence Handle = VK_NULL_HANDLE;
	};

	class Semaphore
	{
	public:
		Semaphore() = default;
		Semaphore(VkDevice LogicalDevice);
		void Create(VkDevice LogicalDevice);
		void Destroy(VkDevice LogicalDevice);
		VkSemaphore Handle = VK_NULL_HANDLE;
	};

	class TimelineSemaphore
	{
	public:
		TimelineSemaphore() = default;
		TimelineSemaphore(VkDevice LogicalDevice, uint64_t InitialValue);
		void Create(VkDevice LogicalDevice, uint64_t InitialValue);
		void Destroy(VkDevice LogicalDevice);

		uint64_t GetSemaphoreCounterValue(VkDevice LogicalDevice);
		void Signal(VkDevice LogicalDevice, uint64_t SignalValue);

		VkSemaphore Handle = VK_NULL_HANDLE;
	};

	void WaitSemaphores(
		VkDevice LogicalDevice,
		VkSemaphore* Semaphores, 
		uint32_t SemaphoreCount,
		uint64_t *WaitValues,
		uint64_t TimeOut = UINT64_MAX
	);

	VkTimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo(
		uint64_t* WaitSemaphoreValues,
		uint32_t WaitSemaphoreValueCount,
		uint64_t* SignalSemaphoreValues,
		uint32_t SignalSemaphoreValueCount
	);
}