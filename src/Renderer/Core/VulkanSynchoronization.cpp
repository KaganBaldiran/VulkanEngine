#include "VulkanSynchoronization.hpp"
#include "VulkanImage.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"

#include <vulkan/vk_enum_string_helper.h>

RENDERER_CORE::PipelineBarrier2::PipelineBarrier2()
{
	ImageMemoryBarriers.reserve(15);
	BufferMemoryBarriers.reserve(15);
}

void RENDERER_CORE::PipelineBarrier2::AppendImageMemoryBarrier(
	VkImage Image,
	VkPipelineStageFlags2 SrcStageMask,
	VkPipelineStageFlags2 DstStageMask,
	VkAccessFlags2 SrcAccessMask,
	VkAccessFlags2 DstAccessMask,
	VkImageLayout OldLayout,
	VkImageLayout NewLayout,
	uint32_t SrcQueueFamilyIndex,
	uint32_t DstQueueFamilyIndex,
	VkImageAspectFlags AspectMask,
	uint32_t BaseMipLevel,
	uint32_t LevelCount,
	uint32_t BaseArrayLayer,
	uint32_t LayerCount
)
{
	VkImageMemoryBarrier2 ImageBarrier{};
	ImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ImageBarrier.image = Image;
	ImageBarrier.srcStageMask = SrcStageMask;
	ImageBarrier.dstStageMask = DstStageMask;
	ImageBarrier.srcAccessMask = SrcAccessMask;
	ImageBarrier.dstAccessMask = DstAccessMask;
	ImageBarrier.oldLayout = OldLayout;
	ImageBarrier.newLayout = NewLayout;
	ImageBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
	ImageBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;
	ImageBarrier.subresourceRange.aspectMask = AspectMask;
	ImageBarrier.subresourceRange.baseMipLevel = BaseMipLevel;
	ImageBarrier.subresourceRange.levelCount = LevelCount;
	ImageBarrier.subresourceRange.baseArrayLayer = BaseArrayLayer;
	ImageBarrier.subresourceRange.layerCount = LayerCount;
	ImageBarrier.pNext = nullptr;
	ImageMemoryBarriers.push_back(ImageBarrier);
}

void RENDERER_CORE::PipelineBarrier2::AppendBufferMemoryBarrier(
	VkBuffer Buffer,
	VkDeviceSize Offset,
	VkDeviceSize Size,
	VkPipelineStageFlags2 SrcStageMask,
	VkPipelineStageFlags2 DstStageMask,
	VkAccessFlags2 SrcAccessMask,
	VkAccessFlags2 DstAccessMask,
	uint32_t SrcQueueFamilyIndex,
	uint32_t DstQueueFamilyIndex
)
{
	VkBufferMemoryBarrier2 BufferBarrier{};
	BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	BufferBarrier.pNext = nullptr;
	BufferBarrier.buffer = Buffer;
	BufferBarrier.srcAccessMask = SrcAccessMask;
	BufferBarrier.dstAccessMask = DstAccessMask;
	BufferBarrier.srcStageMask = SrcStageMask;
	BufferBarrier.dstStageMask = DstStageMask;
	BufferBarrier.offset = Offset;
	BufferBarrier.size = Size;
	BufferBarrier.srcQueueFamilyIndex = SrcQueueFamilyIndex;
	BufferBarrier.dstQueueFamilyIndex = DstQueueFamilyIndex;
	BufferMemoryBarriers.push_back(BufferBarrier);
}

void RENDERER_CORE::PipelineBarrier2::ExecutePipelineBarrier(VkCommandBuffer CommandBuffer)
{
	if (ImageMemoryBarriers.empty() && BufferMemoryBarriers.empty()) return;

	VkDependencyInfo DependencyInfo{};
	DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	DependencyInfo.pNext = nullptr;
	DependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(ImageMemoryBarriers.size());
	DependencyInfo.pImageMemoryBarriers = ImageMemoryBarriers.data();
	DependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(BufferMemoryBarriers.size());
	DependencyInfo.pBufferMemoryBarriers = BufferMemoryBarriers.data();
	DependencyInfo.memoryBarrierCount = 0;
	DependencyInfo.pMemoryBarriers = nullptr;

	vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);
	ImageMemoryBarriers.clear();
	BufferMemoryBarriers.clear();
}

void RENDERER_CORE::SafeImageBarrier(
	VkImage& Image,
	BarrierState &State,
	PipelineBarrier2& Barrier,
	VkImageLayout DestinationLayout,
	VkPipelineStageFlags2 DstStage,
	VkAccessFlags2 DstAccesMask,
	uint32_t SrcQueueFamilyIndex,
	uint32_t DstQueueFamilyIndex,
	VkImageAspectFlags AspectMask
)
{
	if (State.ImageLayout != DestinationLayout || State.StageMask != DstStage || State.AccessMask != DstAccesMask)
	{
		Barrier.AppendImageMemoryBarrier(
			Image,
			State.StageMask,
			DstStage,
			State.AccessMask,
			DstAccesMask,
			State.ImageLayout,
			DestinationLayout,
			SrcQueueFamilyIndex,
			DstQueueFamilyIndex,
			AspectMask
		);
		State.StageMask = DstStage;
		State.ImageLayout = DestinationLayout;
		State.AccessMask = DstAccesMask;
	}
}

void RENDERER_CORE::WaitSemaphores(
	VkDevice LogicalDevice,
	VkSemaphore* Semaphores, 
	uint32_t SemaphoreCount, 
	uint64_t* WaitValues,
	uint64_t TimeOut
)
{
	VkSemaphoreWaitInfo WaitInfo{};
	WaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	WaitInfo.pNext = nullptr;
	WaitInfo.flags = 0;
	WaitInfo.semaphoreCount = SemaphoreCount;
	WaitInfo.pSemaphores = Semaphores;
	WaitInfo.pValues = WaitValues;

	vkWaitSemaphores(LogicalDevice, &WaitInfo, TimeOut);
}

VkTimelineSemaphoreSubmitInfo RENDERER_CORE::TimelineSemaphoreSubmitInfo(
	uint64_t *WaitSemaphoreValues,
    uint32_t WaitSemaphoreValueCount,
	uint64_t *SignalSemaphoreValues,
	uint32_t SignalSemaphoreValueCount
)
{
	VkTimelineSemaphoreSubmitInfo TimelineInfo{};
	TimelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	TimelineInfo.pNext = nullptr;
	TimelineInfo.waitSemaphoreValueCount = WaitSemaphoreValueCount;
	TimelineInfo.pWaitSemaphoreValues = WaitSemaphoreValues;
	TimelineInfo.signalSemaphoreValueCount = SignalSemaphoreValueCount;
	TimelineInfo.pSignalSemaphoreValues = SignalSemaphoreValues;

	return TimelineInfo;
}

RENDERER_CORE::Semaphore::Semaphore(VkDevice LogicalDevice)
{
	Create(LogicalDevice);
}

RENDERER_CORE::Fence::Fence(VkDevice LogicalDevice, VkFenceCreateFlags Flags)
{
	Create(LogicalDevice, Flags);
}

void RENDERER_CORE::Fence::Create(VkDevice LogicalDevice, VkFenceCreateFlags Flags)
{
	VkFenceCreateInfo FenceCreateInfo{};
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceCreateInfo.flags = Flags;

	if (vkCreateFence(LogicalDevice, &FenceCreateInfo, nullptr, &Handle) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create the semaphores and the fence!");
	}
}

RENDERER_CORE::TimelineSemaphore::TimelineSemaphore(VkDevice LogicalDevice, uint64_t InitialValue)
{
	Create(LogicalDevice, InitialValue);
}

void RENDERER_CORE::TimelineSemaphore::Create(VkDevice LogicalDevice,uint64_t InitialValue)
{
	VkSemaphoreTypeCreateInfo TimelineTypeInfo{};
	TimelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	TimelineTypeInfo.pNext = nullptr;
	TimelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	TimelineTypeInfo.initialValue = InitialValue;

	VkSemaphoreCreateInfo SemaphoreCreateInfo{};
	SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	SemaphoreCreateInfo.pNext = &TimelineTypeInfo;
	SemaphoreCreateInfo.flags = 0;

	if (vkCreateSemaphore(LogicalDevice, &SemaphoreCreateInfo, nullptr, &Handle) != VK_SUCCESS)
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR,"Failed to create a timeline semaphore!");
		throw std::runtime_error("Failed to create a timeline semaphore!");
	}
}

void RENDERER_CORE::TimelineSemaphore::Destroy(VkDevice LogicalDevice)
{
	if (Handle != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(LogicalDevice, Handle, nullptr);
		Handle = VK_NULL_HANDLE;
	}
}

uint64_t RENDERER_CORE::TimelineSemaphore::GetSemaphoreCounterValue(VkDevice LogicalDevice)
{
	uint64_t Value;
	vkGetSemaphoreCounterValue(LogicalDevice, Handle, &Value);
	return Value;
}

void RENDERER_CORE::TimelineSemaphore::Signal(VkDevice LogicalDevice,uint64_t SignalValue)
{
	VkSemaphoreSignalInfo SignalInfo{};
	SignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	SignalInfo.pNext = nullptr;
	SignalInfo.semaphore = Handle;
	SignalInfo.value = SignalValue;

	vkSignalSemaphore(LogicalDevice, &SignalInfo);
}

void RENDERER_CORE::Semaphore::Create(VkDevice LogicalDevice)
{
	VkSemaphoreCreateInfo SemaphoreCreateInfo{};
	SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(LogicalDevice, &SemaphoreCreateInfo, nullptr, &Handle) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create the semaphores and the fence!");
	}
}

void RENDERER_CORE::Semaphore::Destroy(VkDevice LogicalDevice)
{
	if (!Handle) return;
	vkDestroySemaphore(LogicalDevice, Handle, nullptr);
	Handle = VK_NULL_HANDLE;
}

void RENDERER_CORE::Fence::Destroy(VkDevice LogicalDevice)
{
	if (!Handle) return;
	vkDestroyFence(LogicalDevice, Handle, nullptr);
	Handle = VK_NULL_HANDLE;
}
