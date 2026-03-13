#include "ResourceManager.hpp"

#include "../Renderer/RendererContext.hpp"

RENDERER::ResourceManager::ResourceManager(RENDERER::RendererContext& RendererContext)
{
	Create(RendererContext);
}

void RENDERER::ResourceManager::Create(RENDERER::RendererContext& RendererContext)
{
	RendererContextPtr = &RendererContext;

	this->TextureManager.Create(RendererContext);
	this->MeshManager.Create(TextureManager, RendererContext);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		PendingCopyOperations[i].reserve(100);
	}

	IsDestroyed = false;
	DestructionPriority = 2;
	COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::ResourceManager::Destroy()
{
	if (IsDestroyed) return;
	for (uint32_t i = 0; i < StagingBuffers.size(); i++)
	{
		StagingBuffers[i].StagingBuffer.Buffer.Destroy(RendererContextPtr->DeviceContext.LogicalDevice);
	}
	IsDestroyed = true;
}

void RENDERER::ResourceManager::AppendModelImportTask(ModelImportInfo ImportInfo)
{
	MeshManager.AppendImportTask(ImportInfo);
}

void RENDERER::ResourceManager::AppendTextureImportTask(TextureImportInfo ImportInfo)
{
	TextureManager.AppendImportTask(ImportInfo);
}

void RENDERER::ResourceManager::SubmitTextureImports()
{
	TextureManager.SubmitImport();
}

void RENDERER::ResourceManager::SubmitModelImports()
{
	MeshManager.SubmitImport();
}

void RENDERER::ResourceManager::WaitModelImportsIdle()
{
	MeshManager.WaitImportsIdle();
}

void RENDERER::ResourceManager::WaitTextureImportsIdle()
{
	TextureManager.WaitImportsIdle();
}

size_t RENDERER::ResourceManager::RequestCopyOperation(
	RENDERER_CORE::QueueType QueueType,
	VkBuffer DestinationBuffer, 
	uint32_t FrameIndex,
	VkPipelineStageFlags2 SrcStageMask,
	VkPipelineStageFlags2 DstStageMask,
	VkAccessFlags2 SrcAccessMask,
	VkAccessFlags2 DstAccessMask
)
{
	auto& CurrentCopyInfoList = CopyInfos[FrameIndex];
	auto &Iterator = CurrentCopyInfoList.end();
	if (DestinationBuffer != VK_NULL_HANDLE)
	{
		Iterator = std::find_if(CurrentCopyInfoList.begin(), CurrentCopyInfoList.end(), [&](CopyOperationEntry& Info) {
			return Info.CopyInfo.DestinationBuffer == DestinationBuffer;
		});
	}
	//No such copy operation entry exists, create one.
	if (Iterator == CurrentCopyInfoList.end())
	{
		CopyOperationEntry NewEntry{};
		NewEntry.CopyInfo.DestinationBuffer = DestinationBuffer;
		NewEntry.CopyInfo.SourceBuffer = StagingBuffers[FrameIndex].StagingBuffer.Buffer.Buffer.BufferObject;
		NewEntry.BufferState.SrcStageMask = SrcStageMask;
		NewEntry.BufferState.DstStageMask = DstStageMask;
		NewEntry.BufferState.SrcAccessMask = SrcAccessMask;
		NewEntry.BufferState.DstAccessMask = DstAccessMask;
		CurrentCopyInfoList.push_back(std::move(NewEntry));
		return CurrentCopyInfoList.size() - 1;
	}
	//If not at least update the buffer barrier state
	Iterator->BufferState.SrcStageMask = SrcStageMask;
	Iterator->BufferState.DstStageMask = DstStageMask;
	Iterator->BufferState.SrcAccessMask = SrcAccessMask;
	Iterator->BufferState.DstAccessMask = DstAccessMask;
	return std::distance(CurrentCopyInfoList.begin(), Iterator);
}

RENDERER::CopyOperationEntry* RENDERER::ResourceManager::GetCopyOperationEntry(const size_t &Index,const uint32_t &FrameIndex)
{
	return &CopyInfos[FrameIndex][Index];
}

void RENDERER::ResourceManager::SetCopyOperationDirty(size_t Index, uint32_t FrameIndex)
{
	PendingCopyOperations[FrameIndex].push_back(Index);
}

void RENDERER::ResourceManager::HandleCopyOperations(
	VkCommandBuffer &CommandBuffer,
	size_t FrameIndex,
    RENDERER_CORE::PipelineBarrier2 &PipelineBarrier2
)
{
	if (PendingCopyOperations[FrameIndex].empty()) return;
	//Traverse the dirty copy operations
	for (auto& PendingOperationIndex : PendingCopyOperations[FrameIndex])
	{
		if (!CopyInfos[FrameIndex].valid(PendingOperationIndex)) continue;
		auto& [QueueType,CopyInfo,BufferBarrierState,Semaphore,DependentOperations] = CopyInfos[FrameIndex][PendingOperationIndex];
		if (CopyInfo.CopyRegions.empty()) continue;

		vkCmdCopyBuffer(
			CommandBuffer,
			StagingBuffers[FrameIndex].StagingBuffer.Buffer.Buffer.BufferObject, //TODO Too much redirection man, gotta do something about this
			CopyInfo.DestinationBuffer,
			static_cast<uint32_t>(CopyInfo.CopyRegions.size()),
			CopyInfo.CopyRegions.data()
		);
		//Buffer memory barrier to make it visible
		PipelineBarrier2.AppendBufferMemoryBarrier(
			CopyInfo.DestinationBuffer,
			0,
			VK_WHOLE_SIZE,
			BufferBarrierState.SrcStageMask,
			BufferBarrierState.DstStageMask,
			BufferBarrierState.SrcAccessMask,
			BufferBarrierState.DstAccessMask
		);
		CopyInfo.CopyRegions.clear();
	}
	PendingCopyOperations[FrameIndex].clear();
	auto& StagingBufferAllocator = StagingBuffers[FrameIndex].StagingBuffer.Allocator;
	StagingBufferAllocator.Reset(StagingBufferAllocator.GetCapacity());
}

