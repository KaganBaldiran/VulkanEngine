#pragma once
#include <vector>
#include <unordered_map>
#include <functional>

#include "Core/VulkanBuffer.hpp"
#include "Core/VulkanImage.hpp"
#include "Core/VulkanSynchoronization.hpp"

namespace RENDERER
{
	class FrameGraph;

	enum ResourceUsageType
	{
		RESOURCE_USAGE_TYPE_BUFFER = 0,
		RESOURCE_USAGE_TYPE_TEXTURE = 1,
		RESOURCE_USAGE_TYPE_UNDEFINED = 2
	};

	struct ResourceUsage
	{
		ResourceUsageType UsageType = RESOURCE_USAGE_TYPE_UNDEFINED;
		void* Resource = nullptr;
		VkPipelineStageFlags2 StageMask = VK_PIPELINE_STAGE_2_NONE;
		VkAccessFlags2 AccessMask = VK_ACCESS_2_NONE;
		VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	};

	struct BufferUsage
	{
		RENDERER_CORE::Buffer* Buffer = nullptr;
		VkPipelineStageFlags2 StageMask = VK_PIPELINE_STAGE_2_NONE;
		VkAccessFlags2 AccessMask = VK_ACCESS_2_NONE;
		uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	};

	struct TextureUsage
	{
		RENDERER_CORE::ImageData* Texture = nullptr;
		VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags2 StageMask = VK_PIPELINE_STAGE_2_NONE;
		VkAccessFlags2 AccessMask = VK_ACCESS_2_NONE;
		VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	};

	class PassBuilder
	{
		friend class FrameGraph;
	private:
		std::vector<BufferUsage> BufferReads;
		std::vector<BufferUsage> BufferWrites;
		std::vector<TextureUsage> TextureReads;
		std::vector<TextureUsage> TextureWrites;
		void Reset();
	public:
		void Read(RENDERER_CORE::Buffer* Buffer, VkPipelineStageFlags2 StageMask, VkAccessFlags2 AccessMask);
		void Read(
			RENDERER_CORE::ImageData* Texture,
			VkImageLayout ImageLayout,
			VkPipelineStageFlags2 Stage,
			VkAccessFlags2 AccessMask,
			VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
		);
		void Write(RENDERER_CORE::Buffer* Buffer, VkPipelineStageFlags2 StageMask, VkAccessFlags2 AccessMask);
		void Write(
			RENDERER_CORE::ImageData* Texture, 
			VkImageLayout ImageLayout, 
			VkPipelineStageFlags2 Stage, 
			VkAccessFlags2 AccessMask,
			VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
		);
	};

	struct FrameGraphTask
	{
		std::function<void(PassBuilder& Builder)> Setup;
		std::function<void(VkCommandBuffer CommandBuffer,uint32_t CurrentFrame)> Task;
		std::string Name;
	};

	struct FrameGraphNode
	{
		std::function<void(VkCommandBuffer CommandBuffer, uint32_t CurrentFrame)> Task;
		uint32_t Degree = 0;
		std::vector<uint32_t> DependentTasks;
		RENDERER_CORE::PipelineBarrier2 Barrier;
		PassBuilder Builder;
		std::string Name;
	};

	struct ResourceAccessHistory
	{
		uint32_t LastWriter;
		std::vector<uint32_t> LastReaders;
	};

	class FrameGraph
	{
	public:
		void AppendTask(FrameGraphTask NewTask);
		void Compile(uint32_t CurrentFrame);
		void Execute(VkCommandBuffer CommandBuffer, uint32_t CurrentFrame);
	private:
		std::vector<FrameGraphTask> Tasks;
		std::vector<FrameGraphNode> Nodes;

		std::unordered_map<void*, ResourceUsage> ResourceUsages;
		std::unordered_map<void*, ResourceAccessHistory> ResourceAccessHistories;
	};
}