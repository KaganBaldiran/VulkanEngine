#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <functional>
#include <vector>
#include <map>

namespace VKCORE
{
	class SwapChain;
	class Window;

	struct FrameSyncObjects
	{
		VkSemaphore ImageAvailableSemaphore;
		VkSemaphore RenderFinishedSemaphore;
		VkFence Fence;
		VkFenceCreateFlags FenceCreateFlag;
	};

	void AllocateFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects);
	void DestroyFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects);

	void SubmitQueue(
		VkQueue& Queue,
		const std::vector<VkSemaphore>& WaitSemaphores,
		const std::vector<VkPipelineStageFlags>& WaitDstStageMask,
		const std::vector<VkCommandBuffer>& CommandBuffers,
		const std::vector<VkSemaphore>& SignalSemaphores,
		VkFence Fence
	);

	VkResult PresentQueue(
		VkQueue& Queue,
		const std::vector<VkSwapchainKHR>& SwapChains,
		const std::vector<VkSemaphore>& WaitSemaphores,
		const std::vector<uint32_t>& ImageIndices,
		const std::vector<VkCommandBuffer>& CommandBuffers
	);

	void RenderFrame(
		VkDevice& LogicalDevice,
		VkQueue& GraphicsQueue,
		VkQueue& PresentQueue,
		std::vector<VkCommandBuffer>& CommandBuffers,
		std::multimap<int, std::function<void(VkCommandBuffer& CurrentCommandBuffer, uint32_t CurrentImageIndex, uint32_t CurrentFrame)>> CommandBufferRecords,
		std::function<void()> OnSwapChainRecreate,
		std::vector<VKCORE::FrameSyncObjects>& SyncObjects,
		SwapChain& DestinationSwapChain,
		Window& window,
		uint32_t& CurrentFrame,
		uint32_t MaxImagesInFlight
	);

	class DynamicRenderingPass
	{
	public:
		DynamicRenderingPass() = default;
		void AppendAttachment(
			VkImageView imageView,
			VkImageLayout imageLayout,
			VkAttachmentLoadOp loadOp,
			VkAttachmentStoreOp storeOp,
			const VkClearValue& clearValue
		);
		void BeginRendering(const VkCommandBuffer& CommandBuffer,VkRect2D& RenderArea);
		void EndRendering(const VkCommandBuffer& CommandBuffer);

	private:
		std::vector<VkRenderingAttachmentInfo> RenderingColorAttachments;
		VkRenderingAttachmentInfo DepthAttachment;
		VkRenderingAttachmentInfo StencilAttachment;
	};

}