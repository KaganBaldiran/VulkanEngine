#pragma once
#include "Core/VulkanCommandBuffer.hpp"
#include "Core/VulkanCommandPool.hpp"
#include "Core/VulkanDevice.hpp"

#include <array>
#include <deque>

namespace RENDERER
{
	struct TrackedCommandBuffer
	{
		uint64_t TimelineValue = 0;
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
	};

	class CommandBufferPool
	{
	public:
		CommandBufferPool(VkDevice LogicalDevice);
		CommandBufferPool() = default;
		void Create(VkDevice LogicalDevice);
		void Destroy(VkDevice LogicalDevice);

		VkCommandBuffer BeginCommandBuffer();
		void EndCommandBuffer(VkCommandBuffer CommandBuffer);
	private:
		std::array<std::vector<TrackedCommandBuffer>, RENDERER_CORE::QUEUE_TYPE_SIZE> CommandBuffers;
		std::array<RENDERER_CORE::CommandPool, RENDERER_CORE::QUEUE_TYPE_SIZE> CommandPools;
	};
}
