#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include "VulkanUtils.hpp" 
#include <functional>
#include <vector>
#include <array>
#include <map>

#include "VulkanSynchoronization.hpp"
#include "../../Common/CommonDefinitions.hpp"

namespace RENDERER_CORE
{
	class SwapChain;
	class Window;

	struct FrameSyncObjects
	{
		VkSemaphore ImageAvailableSemaphore;
		VkSemaphore RenderFinishedSemaphore;
		VkFence Fence;
		VkFenceCreateFlags FenceCreateFlag;
		uint64_t TimelineCounterTarget = 0;
	};
	void AllocateFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects);
	void DestroyFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects);

	void SubmitQueue(
		VkQueue& Queue,
		const std::vector<VkSemaphore>& WaitSemaphores,
		const std::vector<VkPipelineStageFlags>& WaitDstStageMask,
		const std::vector<VkCommandBuffer>& CommandBuffers,
		const std::vector<VkSemaphore>& SignalSemaphores,
		VkFence Fence,
		void* pNext = nullptr
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
		std::vector<RENDERER_CORE::FrameSyncObjects>& SyncObjects,
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
			const VkClearValue& clearValue,
			VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE,
			VkImageView resolveImageView = VK_NULL_HANDLE,
			VkImageLayout resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
		);
		void BeginRendering(const VkCommandBuffer& CommandBuffer,VkRect2D& RenderArea);
		void EndRendering(const VkCommandBuffer& CommandBuffer);

	private:
		std::vector<VkRenderingAttachmentInfo> RenderingColorAttachments;
		VkRenderingAttachmentInfo DepthAttachment;
		VkRenderingAttachmentInfo StencilAttachment;

		bool HaveDepthAttachment = false;
	};

	class FrameManager
	{
	public:
		FrameManager() = default;
		FrameManager(VkDevice LogicalDevice, VkCommandPool DestinationCommandPool);
		void Create(VkDevice LogicalDevice, VkCommandPool DestinationCommandPool);
		void Destroy(VkDevice LogicalDevice);

		VkCommandBuffer BeginFrame(
			VkDevice LogicalDevice,
			VkSwapchainKHR DestinationSwapChain,
			TimelineSemaphore& TimelineSemaphore
		);
		VkResult EndFrame(
			VkDevice LogicalDevice,
			VkQueue PresentQueue,
			VkSwapchainKHR DestinationSwapChain,
			Window& Window
		);
		
		std::array<FrameSyncObjects, MAX_FRAMES_IN_FLIGHT> SyncObjects;
		std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> CommandBuffers;
		uint32_t CurrentFrame = 0;
		uint32_t ImageIndex = 0;
	private:
	}; 
}