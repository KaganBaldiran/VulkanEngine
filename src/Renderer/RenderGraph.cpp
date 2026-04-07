#include "RenderGraph.hpp"

#include "../Common/Log.hpp"
#include <vulkan/vk_enum_string_helper.h>

#include <unordered_set>
#include <queue>

void RENDERER::PassBuilder::Read(RENDERER_CORE::Buffer* Buffer, VkPipelineStageFlags2 StageMask, VkAccessFlags2 AccessMask)
{
	BufferReads.push_back({ Buffer , StageMask, AccessMask });
}

void RENDERER::PassBuilder::Read(
	RENDERER_CORE::ImageData* Texture,
	VkImageLayout ImageLayout,
	VkPipelineStageFlags2 Stage,
	VkAccessFlags2 AccessMask,
	VkImageAspectFlags AspectMask,
	uint32_t QueueFamilyIndex 
)
{
	TextureReads.push_back({ Texture , ImageLayout, Stage,AccessMask,AspectMask,QueueFamilyIndex });
}

void RENDERER::PassBuilder::Write(RENDERER_CORE::Buffer* Buffer, VkPipelineStageFlags2 StageMask, VkAccessFlags2 AccessMask)
{
	BufferWrites.push_back({ Buffer , StageMask, AccessMask });
}

void RENDERER::PassBuilder::Write(
	RENDERER_CORE::ImageData* Texture,
	VkImageLayout ImageLayout,
	VkPipelineStageFlags2 Stage,
	VkAccessFlags2 AccessMask,
	VkImageAspectFlags AspectMask,
	uint32_t QueueFamilyIndex
)
{
	TextureWrites.push_back({ Texture , ImageLayout, Stage,AccessMask,AspectMask,QueueFamilyIndex });
}

void RENDERER::PassBuilder::Reset()
{
	BufferReads.clear();
	TextureReads.clear();
	BufferWrites.clear();
	TextureWrites.clear();
}

void RENDERER::FrameGraph::AppendTask(FrameGraphTask NewTask)
{
	Tasks.push_back(std::move(NewTask));
}


void RENDERER::FrameGraph::Compile(uint32_t CurrentFrame)
{
	Nodes.clear();
	for (uint64_t TaskId = 0; TaskId < Tasks.size(); TaskId++)
	{
		auto& Task = Tasks[TaskId];
		FrameGraphNode NewNode;
		NewNode.Task = std::move(Task.Task);
		NewNode.Name = Task.Name;

		std::unordered_set<uint32_t> DependentWriters;

		Task.Setup(NewNode.Builder);

		for (auto& BufferRead : NewNode.Builder.BufferReads)
		{
			if (ResourceAccessHistories.count(BufferRead.Buffer))
			{
				auto& AccessHistory = ResourceAccessHistories[BufferRead.Buffer];
				if (AccessHistory.LastWriter != UINT32_MAX) DependentWriters.insert(AccessHistory.LastWriter);

				AccessHistory.LastReaders.push_back(TaskId);
			}
			else
			{
				ResourceAccessHistory NewAccessHistory;
				NewAccessHistory.LastWriter = UINT32_MAX;
				NewAccessHistory.LastReaders.push_back(TaskId);
				ResourceAccessHistories[BufferRead.Buffer] = std::move(NewAccessHistory);
			}
		}

		for (auto& TextureRead : NewNode.Builder.TextureReads)
		{
			if (ResourceAccessHistories.count(TextureRead.Texture))
			{
				auto& AccessHistory = ResourceAccessHistories[TextureRead.Texture];
				if (AccessHistory.LastWriter != UINT32_MAX) DependentWriters.insert(AccessHistory.LastWriter);

				AccessHistory.LastReaders.push_back(TaskId);
			}
			else
			{
				ResourceAccessHistory NewAccessHistory;
				NewAccessHistory.LastWriter = UINT32_MAX;
				NewAccessHistory.LastReaders.push_back(TaskId);
				ResourceAccessHistories[TextureRead.Texture] = std::move(NewAccessHistory);
			}
		}

		for (auto& TextureWrite : NewNode.Builder.TextureWrites)
		{
			if (ResourceAccessHistories.count(TextureWrite.Texture))
			{
				auto& AccessHistory = ResourceAccessHistories[TextureWrite.Texture];
				
				if (AccessHistory.LastWriter != UINT32_MAX) DependentWriters.insert(AccessHistory.LastWriter);
				AccessHistory.LastWriter = TaskId;

				for (auto& LastReader : AccessHistory.LastReaders)
				{
					DependentWriters.insert(LastReader);
				}
				AccessHistory.LastReaders.clear();
			}
			else
			{
				ResourceAccessHistory NewAccessHistory;
				NewAccessHistory.LastWriter = TaskId;
				ResourceAccessHistories[TextureWrite.Texture] = std::move(NewAccessHistory);
			}
		}

		for (auto& BufferWrite : NewNode.Builder.BufferWrites)
		{
			if (ResourceAccessHistories.count(BufferWrite.Buffer))
			{
				auto& AccessHistory = ResourceAccessHistories[BufferWrite.Buffer];

				if (AccessHistory.LastWriter != UINT32_MAX) DependentWriters.insert(AccessHistory.LastWriter);
				AccessHistory.LastWriter = TaskId;

				for (auto& LastReader : AccessHistory.LastReaders)
				{
					DependentWriters.insert(LastReader);
				}
				AccessHistory.LastReaders.clear();
			}
			else
			{
				ResourceAccessHistory NewAccessHistory;
				NewAccessHistory.LastWriter = TaskId;
				ResourceAccessHistories[BufferWrite.Buffer] = std::move(NewAccessHistory);
			}
		}

		for (auto& Writer : DependentWriters)
		{
			if (Writer == UINT32_MAX || Writer == TaskId) continue;
			Nodes[Writer].DependentTasks.push_back(TaskId);
			NewNode.Degree++;
		}

		Nodes.push_back(std::move(NewNode));
	}

	std::vector<FrameGraphNode> SortedNodes;
	SortedNodes.reserve(Nodes.size());
	std::queue<uint32_t> ProcessQueue;

	for (uint64_t NodesID = 0; NodesID < Nodes.size(); NodesID++)
	{
		if (Nodes[NodesID].Degree == 0) ProcessQueue.push(NodesID);
	}

	while (ProcessQueue.size())
	{
		auto& CurrentNode = Nodes[ProcessQueue.front()];
		ProcessQueue.pop();

		for (auto& DependentNodeId : CurrentNode.DependentTasks)
		{
			auto& DependentNode = Nodes[DependentNodeId];

			DependentNode.Degree--;
			if (DependentNode.Degree == 0)
			{
				ProcessQueue.push(DependentNodeId);
			}
		}
		SortedNodes.push_back(std::move(CurrentNode));
	}
	std::swap(SortedNodes, Nodes);

	if (Nodes.size() != Tasks.size())
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_ERROR, "Frame graph detected deadlock!");
	}

	for (auto& Node : Nodes)
	{
		for (auto& BufferRead : Node.Builder.BufferReads)
		{
			if (ResourceUsages.count(BufferRead.Buffer))
			{
				auto& Usage = ResourceUsages[BufferRead.Buffer];

				if (Usage.StageMask != BufferRead.StageMask || Usage.AccessMask != BufferRead.AccessMask)
				{
					Node.Barrier.AppendBufferMemoryBarrier(
						BufferRead.Buffer->BufferObject,
						0,
						VK_WHOLE_SIZE,
						Usage.StageMask,
						BufferRead.StageMask,
						Usage.AccessMask,
						BufferRead.AccessMask
					);
					Usage.StageMask = BufferRead.StageMask;
					Usage.AccessMask = BufferRead.AccessMask;
				}
			}
			else
			{
				Node.Barrier.AppendBufferMemoryBarrier(
					BufferRead.Buffer->BufferObject,
					0,
					VK_WHOLE_SIZE,
					BufferRead.Buffer->BarrierState.StageMask,
					BufferRead.StageMask,
					BufferRead.Buffer->BarrierState.AccessMask,
					BufferRead.AccessMask
				);

				ResourceUsage NewUsage;
				NewUsage.UsageType = RESOURCE_USAGE_TYPE_BUFFER;
				NewUsage.StageMask = BufferRead.StageMask;
				NewUsage.AccessMask = BufferRead.AccessMask;
				NewUsage.Resource = BufferRead.Buffer;
				ResourceUsages[BufferRead.Buffer] = std::move(NewUsage);
			}
		}

		for (auto& TextureRead : Node.Builder.TextureReads)
		{
			if (ResourceUsages.count(TextureRead.Texture))
			{
				auto& Usage = ResourceUsages[TextureRead.Texture];

				if (Usage.StageMask != TextureRead.StageMask || Usage.AccessMask != TextureRead.AccessMask ||
					Usage.ImageLayout != TextureRead.ImageLayout)
				{
					Node.Barrier.AppendImageMemoryBarrier(
						TextureRead.Texture->Image,
						Usage.StageMask,
						TextureRead.StageMask,
						Usage.AccessMask,
						TextureRead.AccessMask,
						Usage.ImageLayout,
						TextureRead.ImageLayout,
						VK_QUEUE_FAMILY_IGNORED,
						VK_QUEUE_FAMILY_IGNORED,
						TextureRead.AspectMask
					);

					Usage.StageMask = TextureRead.StageMask;
					Usage.AccessMask = TextureRead.AccessMask;
					Usage.ImageLayout = TextureRead.ImageLayout;
				}
			}
			else
			{
				Node.Barrier.AppendImageMemoryBarrier(
					TextureRead.Texture->Image,
					TextureRead.Texture->BarrierState.StageMask,
					TextureRead.StageMask,
					TextureRead.Texture->BarrierState.AccessMask,
					TextureRead.AccessMask,
					TextureRead.Texture->BarrierState.ImageLayout,
					TextureRead.ImageLayout,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					TextureRead.AspectMask
				);

				ResourceUsage NewUsage;
				NewUsage.UsageType = RESOURCE_USAGE_TYPE_TEXTURE;
				NewUsage.StageMask = TextureRead.StageMask;
				NewUsage.AccessMask = TextureRead.AccessMask;
				NewUsage.ImageLayout = TextureRead.ImageLayout;
				NewUsage.Resource = TextureRead.Texture;
				ResourceUsages[TextureRead.Texture] = std::move(NewUsage);
			}
		}

		for (auto& TextureWrite : Node.Builder.TextureWrites)
		{
			if (ResourceUsages.count(TextureWrite.Texture))
			{
				auto& Usage = ResourceUsages[TextureWrite.Texture];

				if (Usage.StageMask != TextureWrite.StageMask || Usage.AccessMask != TextureWrite.AccessMask ||
					Usage.ImageLayout != TextureWrite.ImageLayout)
				{
					Node.Barrier.AppendImageMemoryBarrier(
						TextureWrite.Texture->Image,
						Usage.StageMask,
						TextureWrite.StageMask,
						Usage.AccessMask,
						TextureWrite.AccessMask,
						Usage.ImageLayout,
						TextureWrite.ImageLayout,
						VK_QUEUE_FAMILY_IGNORED,
						VK_QUEUE_FAMILY_IGNORED,
						TextureWrite.AspectMask
					);

					Usage.StageMask = TextureWrite.StageMask;
					Usage.AccessMask = TextureWrite.AccessMask;
					Usage.ImageLayout = TextureWrite.ImageLayout;
				}
			}
			else
			{
				Node.Barrier.AppendImageMemoryBarrier(
					TextureWrite.Texture->Image,
					TextureWrite.Texture->BarrierState.StageMask,
					TextureWrite.StageMask,
					TextureWrite.Texture->BarrierState.AccessMask,
					TextureWrite.AccessMask,
					TextureWrite.Texture->BarrierState.ImageLayout,
					TextureWrite.ImageLayout,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					TextureWrite.AspectMask
				);

				ResourceUsage NewUsage;
				NewUsage.UsageType = RESOURCE_USAGE_TYPE_TEXTURE;
				NewUsage.StageMask = TextureWrite.StageMask;
				NewUsage.AccessMask = TextureWrite.AccessMask;
				NewUsage.ImageLayout = TextureWrite.ImageLayout;
				NewUsage.Resource = TextureWrite.Texture;
				ResourceUsages[TextureWrite.Texture] = std::move(NewUsage);
			}
		}

		for (auto& BufferWrite : Node.Builder.BufferWrites)
		{
			if (ResourceUsages.count(BufferWrite.Buffer))
			{
				auto& Usage = ResourceUsages[BufferWrite.Buffer];

				if (Usage.StageMask != BufferWrite.StageMask || Usage.AccessMask != BufferWrite.AccessMask)
				{
					Node.Barrier.AppendBufferMemoryBarrier(
						BufferWrite.Buffer->BufferObject,
						0,
						VK_WHOLE_SIZE,
						Usage.StageMask,
						BufferWrite.StageMask,
						Usage.AccessMask,
						BufferWrite.AccessMask
					);

					Usage.StageMask = BufferWrite.StageMask;
					Usage.AccessMask = BufferWrite.AccessMask;
				}
			}
			else
			{
				Node.Barrier.AppendBufferMemoryBarrier(
					BufferWrite.Buffer->BufferObject,
					0,
					VK_WHOLE_SIZE,
					BufferWrite.Buffer->BarrierState.StageMask,
					BufferWrite.StageMask,
					BufferWrite.Buffer->BarrierState.AccessMask,
					BufferWrite.AccessMask
				);

				ResourceUsage NewUsage;
				NewUsage.UsageType = RESOURCE_USAGE_TYPE_BUFFER;
				NewUsage.StageMask = BufferWrite.StageMask;
				NewUsage.AccessMask = BufferWrite.AccessMask;
				NewUsage.Resource = BufferWrite.Buffer;
				ResourceUsages[BufferWrite.Buffer] = std::move(NewUsage);
			}
		}

	}

	for (auto& [ResourcePtr, Usage] : ResourceUsages)
	{
		switch (Usage.UsageType)
		{
		case RESOURCE_USAGE_TYPE_BUFFER:
		{
			RENDERER_CORE::Buffer* BufferPtr = reinterpret_cast<RENDERER_CORE::Buffer*>(Usage.Resource);
			BufferPtr->BarrierState.AccessMask = Usage.AccessMask;
			BufferPtr->BarrierState.StageMask = Usage.StageMask;
			break;
		}
		case RESOURCE_USAGE_TYPE_TEXTURE:
		{
			RENDERER_CORE::ImageData* TexturePtr = reinterpret_cast<RENDERER_CORE::ImageData*>(Usage.Resource);
			TexturePtr->BarrierState.AccessMask = Usage.AccessMask;
			TexturePtr->BarrierState.StageMask = Usage.StageMask;
			TexturePtr->BarrierState.ImageLayout = Usage.ImageLayout;
			break;
		}
		default:
			break;
		}
	}

	Tasks.clear();
	ResourceAccessHistories.clear();
	ResourceUsages.clear();
}

void RENDERER::FrameGraph::Execute(VkCommandBuffer CommandBuffer,uint32_t CurrentFrame)
{
	for (size_t NodeIndex = 0; NodeIndex < Nodes.size(); NodeIndex++)
	{
		auto& Node = Nodes[NodeIndex];
		Node.Barrier.ExecutePipelineBarrier(CommandBuffer);
		Node.Task(CommandBuffer, CurrentFrame);
	}
}
