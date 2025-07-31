#include "VulkanSynchoronization.hpp"

VKCORE::PipelineBarrier2::PipelineBarrier2()
{
	ImageMemoryBarriers.reserve(15);
	BufferMemoryBarriers.reserve(15);
}

void VKCORE::PipelineBarrier2::AppendImageMemoryBarrier(
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

void VKCORE::PipelineBarrier2::AppendBufferMemoryBarrier(
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

void VKCORE::PipelineBarrier2::ExecutePipelineBarrier(VkCommandBuffer CommandBuffer)
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
