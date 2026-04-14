#include "ResourceManager.hpp"

#include "RendererContext.hpp"
#include "RenderGraph.hpp"
#include "../Common/Log.hpp"

RENDERER::ResourceManager::ResourceManager(RENDERER::RendererContext& RendererContext)
{
	Create(RendererContext);
}

void RENDERER::ResourceManager::Create(RENDERER::RendererContext& RendererContext)
{
	RendererContextPtr = &RendererContext;

	this->TextureManager.Create(RendererContext);
	this->MeshManager.Create(TextureManager, RendererContext,*this);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		StagingBuffers[i].AllocateSceneStagingBuffer(PersistentStagingBufferSize, RendererContextPtr);
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
		RENDERER_CORE::DestroyBuffer(RendererContextPtr->DeviceContext.LogicalDevice, 
											StagingBuffers[i].StagingBuffer.Buffer);
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

void RENDERER::ResourceManager::RequestCopyOperation(
	const std::vector<VkBufferCopy>& CopyRegions,
	RENDERER_CORE::QueueType QueueType,
	RENDERER_CORE::Buffer* DestinationBuffer,
	const std::shared_ptr<DataBlock>& Data,
	uint32_t Priority,
	CopyOperationFlagBit Flags,
	std::shared_ptr<COMMON::AsyncToken>* WaitTokens,
	uint32_t WaitTokenCount,
	std::shared_ptr<COMMON::AsyncToken>* SignalTokens,
	uint32_t SignalTokenCount
)
{
	if (!DestinationBuffer)
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_VERBOSE, "Invalid destination buffer for a copy operation!");
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Invalid destination buffer for a copy operation!");
		throw std::runtime_error("Invalid destination buffer for a copy operation!");
	}
	if (!WaitTokens && WaitTokenCount > 0)
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_ERROR, "Mismatch in given wait token arguments!");
		LOG_FILE(GLOBAL_LOG_FILE_PATH,COMMON::LOG_SEVERITY_ERROR, "Mismatch in given wait token arguments!");
		throw std::runtime_error("Mismatch in given wait token arguments!");
	}
	if (!SignalTokens && SignalTokenCount > 0)
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_ERROR, "Mismatch in given signal token arguments!");
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Mismatch in given signal token arguments!");
		throw std::runtime_error("Mismatch in given signal token arguments!");
	}

	CopyOperationEntry NewEntry;
	NewEntry.Data = Data;
	NewEntry.Flags = Flags;
	NewEntry.DestinationBuffer = DestinationBuffer;
	NewEntry.Priority = Priority;
	NewEntry.QueueType = QueueType;
	NewEntry.CopyRegions = CopyRegions;
	NewEntry.SignalTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(SignalTokens, SignalTokens + SignalTokenCount));
	NewEntry.WaitTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(WaitTokens, WaitTokens + WaitTokenCount));
	NewEntry.State = std::make_shared<COMMON::AsyncToken>();
	this->CopyOperations.push_back(std::move(NewEntry));
	ShouldSortCopyInfos = true;
}

void RENDERER::ResourceManager::HandleCopyOperations(
	VkCommandBuffer &CommandBuffer,
	size_t FrameIndex,
    RENDERER_CORE::PipelineBarrier2 &PipelineBarrier2
)
{
	if (CopyOperations.empty()) return;
	//Traverse the dirty copy operations
	for (auto& CopyOperation : CopyOperations)
	{
		if (CopyOperation.CopyRegions.empty()) continue;

		vkCmdCopyBuffer(
			CommandBuffer,
			StagingBuffers[FrameIndex].StagingBuffer.Buffer.BufferObject, 
			CopyOperation.DestinationBuffer->BufferObject,
			static_cast<uint32_t>(CopyOperation.CopyRegions.size()),
			CopyOperation.CopyRegions.data()
		);
		
		CopyOperation.CopyRegions.clear();
	}
	auto& StagingBufferAllocator = StagingBuffers[FrameIndex].StagingBuffer.Allocator;
	StagingBufferAllocator.Reset(StagingBufferAllocator.GetCapacity());
}

void RENDERER::ResourceManager::QueueCopyOperations(VkCommandBuffer& CommandBuffer, size_t FrameIndex, FrameGraph& FrameGraph)
{
	if (CopyOperations.empty()) return;

	FrameGraph.AppendTask({
		[this, FrameIndex](RENDERER::PassBuilder& Builder) {
			for (auto& CopyOperation : CopyOperations)
			{
				if (CopyOperation.CopyRegions.empty()) continue;
				Builder.Write(
					CopyOperation.DestinationBuffer,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT
				);
			};
		},

		[this](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
			//std::cout << "Pass [CopyOperations_Internal] is being executed..." << std::endl;
			LoadMemoryChunks(CommandBuffer,CurrentFrame);
		},

		"CopyOperations_Internal"
	});

}

void RENDERER::ResourceManager::LoadMemoryChunks(VkCommandBuffer CommandBuffer,uint32_t FrameIndex)
{
	auto& StagingBuffer = StagingBuffers[FrameIndex].StagingBuffer;

	if (ShouldSortCopyInfos)
	{
		std::sort(CopyOperations.begin(), CopyOperations.end(), [&](const CopyOperationEntry& Entry0, const CopyOperationEntry& Entry1) {
			return Entry0.Priority < Entry1.Priority;
		});
	}
	ShouldSortCopyInfos = false;

	uint8_t* StagingBufferMappedPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);
	size_t Offset = 0, Budget = PersistentStagingBufferSize;

	std::vector<VkBufferCopy> AppendedCopyRegions;
	for (size_t CopyInfoIndex = 0; CopyInfoIndex < CopyOperations.size(); CopyInfoIndex++)
	{
		auto& CopyInfo = CopyOperations[CopyInfoIndex];
		if (CopyInfo.CopyRegions.empty()) continue;

		for (auto& WaitToken : CopyInfo.WaitTokens)
		{
			if (!WaitToken->State.load()) continue;
		}

		if (CopyInfo.Flags & COPY_OPERATION_FLAG_ATOMIC)
		{
			size_t Size = 0;
			for (auto& CopyRegion : CopyInfo.CopyRegions)
			{
				Size += CopyRegion.size;
			}
			if (Size > Budget)
			{
				if (Size > PersistentStagingBufferSize)
				{
					LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Size of the data being copied is larger than the specified persistent staging buffer size!");
					throw std::runtime_error("Size of the data being copied is larger than the specified persistent staging buffer size!");
				}
				continue;
			}
		}

		for (size_t CopyRegionIndex = CopyInfo.CurrentCopyRegionInline; CopyRegionIndex < CopyInfo.CopyRegions.size(); CopyRegionIndex++)
		{
			VkBufferCopy& BluePrintCopyRegion = CopyInfo.CopyRegions[CopyRegionIndex];

			VkBufferCopy CopyRegion;
			CopyRegion.srcOffset = Offset;
			CopyRegion.dstOffset = BluePrintCopyRegion.dstOffset;

			size_t OriginalSrcOffset = BluePrintCopyRegion.srcOffset;

			if (Budget >= BluePrintCopyRegion.size)
			{
				CopyRegion.size = BluePrintCopyRegion.size;
				CopyInfo.CurrentCopyRegionInline++;
			}
			else
			{
				LOG_CONSOLE(COMMON::LOG_SEVERITY_DEBUG, "Created a chunk!");
				CopyRegion.size = Budget;
				BluePrintCopyRegion.size -= Budget;
				BluePrintCopyRegion.dstOffset += Budget;
				BluePrintCopyRegion.srcOffset += Budget;
			}
			Budget -= CopyRegion.size;
			Offset += CopyRegion.size;

			memcpy(StagingBufferMappedPtr + CopyRegion.srcOffset, CopyInfo.Data->DataPtr + OriginalSrcOffset, CopyRegion.size);
			AppendedCopyRegions.push_back(CopyRegion);

			if (Budget == 0) break;
		}
		if (!AppendedCopyRegions.empty())
		{
			vkCmdCopyBuffer(
				CommandBuffer,
				StagingBuffer.Buffer.BufferObject,
				CopyInfo.DestinationBuffer->BufferObject,
				static_cast<uint32_t>(AppendedCopyRegions.size()),
				AppendedCopyRegions.data()
			);
			AppendedCopyRegions.clear();
		}
		if (CopyInfo.CurrentCopyRegionInline == CopyInfo.CopyRegions.size())
		{
			CopyInfo.State->State.store(true);

			for (auto& SignalToken : CopyInfo.SignalTokens)
			{
				SignalToken->State.store(true);
			}

			CopyInfo.Data.reset();
			ShouldSortCopyInfos = true;
		}
		if (Budget == 0) break;
	}

	CopyOperations.erase(
		std::remove_if(CopyOperations.begin(), CopyOperations.end(),[](CopyOperationEntry& Entry) {
			return Entry.State->State == true;
		}),
		CopyOperations.end()
	);
}

