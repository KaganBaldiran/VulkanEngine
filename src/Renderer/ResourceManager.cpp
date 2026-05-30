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

	this->TextureManager.Create(RendererContext,*this);
	this->MeshManager.Create(TextureManager, RendererContext,*this);
	Semaphore.Create(RendererContext.DeviceContext.LogicalDevice, 0);

	auto TransferQueue = RendererContext.DeviceContext.GetQueue(RENDERER_CORE::QUEUE_TYPE_TRANSFER);
	if (TransferQueue == VK_NULL_HANDLE || !RendererContext.QueueFamilyIndices.TransferFamily.has_value())
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_DEBUG, "Error during initializing the resource manager! No transfer queue present!");
	}

	for (auto& TransferContext : AsyncTransferContexts)
	{
		TransferContext.CommandPool.Create(
			RendererContext.QueueFamilyIndices.TransferFamily.value(),
			RendererContext.DeviceContext.LogicalDevice
		);

		RENDERER_CORE::AllocateCommandBuffers(
			TransferContext.CommandPool.Handle,
			RendererContextPtr->DeviceContext.LogicalDevice,
			&TransferContext.CommandBuffer,
			1
		);
	}

	for (auto& StagingBuffer : TransientStagingBuffers)
	{
		StagingBuffer.AllocateSceneStagingBuffer(PersistentStagingBufferSize, RendererContextPtr);
	}
	RingStagingBuffer.AllocateSceneStagingBuffer(PersistentStagingBufferSize * 3, RendererContextPtr);

	IsDestroyed = false;
	DestructionPriority = 2;
	COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::ResourceManager::Destroy()
{
	if (IsDestroyed) return;

	for (auto& TransferContext : AsyncTransferContexts)
	{
		TransferContext.CommandPool.Destroy(RendererContextPtr->DeviceContext.LogicalDevice);
	}
	for (auto& StagingBuffer: TransientStagingBuffers)
	{
		RENDERER_CORE::DestroyBuffer(RendererContextPtr->DeviceContext.LogicalDevice, 
										StagingBuffer.StagingBuffer.Buffer);
	}
	RENDERER_CORE::DestroyBuffer(RendererContextPtr->DeviceContext.LogicalDevice,
		RingStagingBuffer.StagingBuffer.Buffer);
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

void RENDERER::ResourceManager::RequestBufferCopyOperation(
	bool Asynchronous,
	const std::vector<VkBufferCopy>& CopyRegions,
	RENDERER_CORE::Buffer* DestinationBuffer,
	const std::shared_ptr<DataBlock>& Data,
	uint32_t Priority,
	CopyOperationFlagBit Flags,
	RENDERER_CORE::BarrierState OnCompleteTransition,
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
	NewEntry.BufferCopyRegions = CopyRegions;
	NewEntry.OnCompleteTransition = OnCompleteTransition;
	NewEntry.SignalTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(SignalTokens, SignalTokens + SignalTokenCount));
	NewEntry.WaitTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(WaitTokens, WaitTokens + WaitTokenCount));
	NewEntry.AllChunksProcessed = std::make_shared<COMMON::AsyncToken>();
	if (Asynchronous)
	{
		this->AsyncCopyOperations.push_back(std::move(NewEntry));
		ShouldSortAsyncCopyInfos = true;
	}
	else
	{
		this->CopyOperations.push_back(std::move(NewEntry));
		ShouldSortCopyInfos = true;
	}
}

void RENDERER::ResourceManager::RequestImageCopyOperation(
	bool Asynchronous,
	const std::vector<VkBufferImageCopy>& CopyRegions,
	RENDERER_CORE::ImageData* DestinationImage,
	const std::shared_ptr<DataBlock>& Data,
	RENDERER_CORE::ImageMetaData ImageMetaData,
	VkImageAspectFlags Aspect,
	uint32_t Priority, 
	CopyOperationFlagBit Flags, 
	RENDERER_CORE::BarrierState OnCompleteTransition,
	std::shared_ptr<COMMON::AsyncToken>* WaitTokens, 
	uint32_t WaitTokenCount, 
	std::shared_ptr<COMMON::AsyncToken>* SignalTokens, 
	uint32_t SignalTokenCount
)
{
	if (!DestinationImage)
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_VERBOSE, "Invalid destination buffer for a copy operation!");
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Invalid destination buffer for a copy operation!");
		throw std::runtime_error("Invalid destination buffer for a copy operation!");
	}
	if (!WaitTokens && WaitTokenCount > 0)
	{
		LOG_CONSOLE(COMMON::LOG_SEVERITY_ERROR, "Mismatch in given wait token arguments!");
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Mismatch in given wait token arguments!");
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
	NewEntry.DestinationImage = DestinationImage;
	NewEntry.Priority = Priority;
	NewEntry.ImageCopyRegions = CopyRegions;
	NewEntry.Aspect = Aspect;
	NewEntry.OnCompleteTransition = std::move(OnCompleteTransition);
	NewEntry.ImageMetaData = std::move(ImageMetaData);
	NewEntry.SignalTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(SignalTokens, SignalTokens + SignalTokenCount));
	NewEntry.WaitTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(WaitTokens, WaitTokens + WaitTokenCount));
	NewEntry.AllChunksProcessed = std::make_shared<COMMON::AsyncToken>();
	if (Asynchronous)
	{
		this->AsyncCopyOperations.push_back(std::move(NewEntry));
		ShouldSortAsyncCopyInfos = true;
	}
	else
	{
		this->CopyOperations.push_back(std::move(NewEntry));
		ShouldSortCopyInfos = true;
	}
}

void RENDERER::ResourceManager::RecordAcquireBarriers(
	VkCommandBuffer DestinationCommandBuffer,
	const std::vector<InFlightPayload> &CompletedTransfers
)
{
	if (CompletedTransfers.empty()) return;

	uint32_t TransferQueueFamily = RendererContextPtr->QueueFamilyIndices.GetQueueFamilyIndex(RENDERER_CORE::QUEUE_TYPE_TRANSFER);
	uint32_t GraphicsQueueFamily = RendererContextPtr->QueueFamilyIndices.GetQueueFamilyIndex(RENDERER_CORE::QUEUE_TYPE_GRAPHICS);

	if (TransferQueueFamily == GraphicsQueueFamily) return;

	RENDERER_CORE::PipelineBarrier2 AcquireBarrier;
	for (const auto& Transfer : CompletedTransfers)
	{
		if (Transfer.DestinationBuffer)
		{
			AcquireBarrier.AppendBufferMemoryBarrier(
				Transfer.DestinationBuffer->BufferObject,
				0, VK_WHOLE_SIZE,
				VK_PIPELINE_STAGE_2_NONE,
				VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				VK_ACCESS_2_NONE,
				VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
				TransferQueueFamily, GraphicsQueueFamily
			);

			Transfer.DestinationBuffer->BarrierState.QueueFamily = GraphicsQueueFamily;
		}
		else if (Transfer.DestinationImage)
		{
			AcquireBarrier.AppendImageMemoryBarrier(
				Transfer.DestinationImage->Image,
				VK_PIPELINE_STAGE_2_NONE,
				VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				VK_ACCESS_2_NONE,
				VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, 
				Transfer.OnCompleteTransition.ImageLayout,
				Transfer.OnCompleteTransition.ImageLayout,
				TransferQueueFamily, GraphicsQueueFamily,
				Transfer.Aspect
			);

			Transfer.DestinationImage->BarrierState.QueueFamily = GraphicsQueueFamily;
			Transfer.DestinationImage->BarrierState.ImageLayout = Transfer.OnCompleteTransition.ImageLayout;
		}
	}

	AcquireBarrier.ExecutePipelineBarrier(DestinationCommandBuffer);
}

void RENDERER::ResourceManager::QueueTransientCopyOperations(VkCommandBuffer& CommandBuffer, size_t FrameIndex, FrameGraph& FrameGraph)
{
	if (CopyOperations.empty()) return;

	std::shared_ptr<FrameCopyLoad> CopyLoad = std::make_shared<FrameCopyLoad>();
	LoadMemoryChunks(FrameIndex, CopyLoad->BufferCopyLoads, CopyLoad->ImageCopyLoads);
	auto& StagingBuffer = TransientStagingBuffers[FrameIndex].StagingBuffer;

	std::vector<InFlightPayload> FramePendingQueueAcquires;
	{
		std::lock_guard<std::mutex> Lock(AcquireQueueMutex);
		FramePendingQueueAcquires.swap(PendingAcquires);
	}

	if (CopyLoad->BufferCopyLoads.empty() && CopyLoad->ImageCopyLoads.empty()) return;
	FrameGraph.AppendTask({
		[this, FrameIndex,CopyLoad](RENDERER::PassBuilder& Builder) {
			for (auto& BufferCopyLoad : CopyLoad->BufferCopyLoads)
			{
				if (BufferCopyLoad.CopyRegions.empty()) continue;
				Builder.Write(
					BufferCopyLoad.BufferPtr,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT
				);
			};

			for (auto& ImageCopyLoad : CopyLoad->ImageCopyLoads)
			{
				if (ImageCopyLoad.CopyRegions.empty()) continue;
				Builder.Write(
					ImageCopyLoad.ImagePtr,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT
				);
			};
		},

		[this,CopyLoad,&StagingBuffer,CurrentPendingAcquires = std::move(FramePendingQueueAcquires)](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
			for (auto& BufferCopyLoad : CopyLoad->BufferCopyLoads)
			{
				if (!BufferCopyLoad.CopyRegions.empty())
				{
					vkCmdCopyBuffer(
						CommandBuffer,
						StagingBuffer.Buffer.BufferObject,
						BufferCopyLoad.BufferPtr->BufferObject,
						static_cast<uint32_t>(BufferCopyLoad.CopyRegions.size()),
						BufferCopyLoad.CopyRegions.data()
					);
				}
			};

			RENDERER_CORE::PipelineBarrier2 Barrier;
			for (auto& ImageCopyLoad : CopyLoad->ImageCopyLoads)
			{
				if (!ImageCopyLoad.CopyRegions.empty())
				{
					vkCmdCopyBufferToImage(
						CommandBuffer,
						StagingBuffer.Buffer.BufferObject,
						ImageCopyLoad.ImagePtr->Image,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						ImageCopyLoad.CopyRegions.size(),
						ImageCopyLoad.CopyRegions.data()
					);

					if (ImageCopyLoad.BarrierState.ImageLayout != VK_IMAGE_LAYOUT_UNDEFINED)
					{
						Barrier.AppendImageMemoryBarrier(
							ImageCopyLoad.ImagePtr->Image,
							ImageCopyLoad.ImagePtr->BarrierState.StageMask,
							ImageCopyLoad.BarrierState.StageMask,
							ImageCopyLoad.ImagePtr->BarrierState.AccessMask,
							ImageCopyLoad.BarrierState.AccessMask,
							ImageCopyLoad.ImagePtr->BarrierState.ImageLayout,
							ImageCopyLoad.BarrierState.ImageLayout,
							VK_QUEUE_FAMILY_IGNORED,
							VK_QUEUE_FAMILY_IGNORED,
							ImageCopyLoad.Aspect
						);
					}
				}
			};
			Barrier.ExecutePipelineBarrier(CommandBuffer);
			RecordAcquireBarriers(CommandBuffer, CurrentPendingAcquires);
		},

		"CopyOperations_Internal"
	});
}

void RENDERER::ResourceManager::LoadMemoryChunks(
	uint32_t FrameIndex,
	std::vector<BufferCopyLoad> &BufferCopyLoads,
	std::vector<ImageCopyLoad> &ImageCopyLoads
)
{
	auto& StagingBuffer = TransientStagingBuffers[FrameIndex].StagingBuffer;

	if (ShouldSortCopyInfos)
	{
		std::sort(CopyOperations.begin(), CopyOperations.end(), [&](const CopyOperationEntry& Entry0, const CopyOperationEntry& Entry1) {
			return Entry0.Priority < Entry1.Priority;
		});
	}
	ShouldSortCopyInfos = false;

	uint8_t* StagingBufferMappedPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);
	size_t Offset = 0, Budget = PersistentStagingBufferSize;

	for (size_t CopyInfoIndex = 0; CopyInfoIndex < CopyOperations.size(); CopyInfoIndex++)
	{
		auto& CopyInfo = CopyOperations[CopyInfoIndex];
		if (CopyInfo.BufferCopyRegions.empty() && CopyInfo.ImageCopyRegions.empty()) continue;

		bool ShouldSkip = false;
		for (auto& WaitToken : CopyInfo.WaitTokens)
		{
			if (!WaitToken->State.load())
			{
				ShouldSkip = true;
				break;
			}
		}
		if (ShouldSkip) continue;

		if (CopyInfo.DestinationBuffer)
		{
			bool Result = ProcessBufferCopyOperation(
				CopyInfo, 
				Budget, 
				Offset, 
				StagingBufferMappedPtr, 
				BufferCopyLoads
			);
			if (!Result) continue;
		}
		else
		{
			bool Result = ProcessImageCopyOperation(
				CopyInfo,
				Budget,
				Offset,
				StagingBufferMappedPtr,
				ImageCopyLoads
			);
			if (!Result) continue;
		}

		if (CopyInfo.AllChunksProcessed->State.load())
		{
			for (auto& SignalToken : CopyInfo.SignalTokens)
			{
				SignalToken->State.store(true);
			}
			ShouldSortCopyInfos = true;
		}

		if (Budget == 0) break;
	}

	CopyOperations.erase(
		std::remove_if(CopyOperations.begin(), CopyOperations.end(),[](CopyOperationEntry& Entry) {
			return Entry.AllChunksProcessed->State == true;
		}),
		CopyOperations.end()
	);
}

bool RENDERER::ResourceManager::ProcessBufferCopyOperation(
      CopyOperationEntry &CopyInfo,
	  size_t &Budget,
	  size_t &Offset,
      uint8_t* StagingBufferMappedPtr,
	  std::vector<BufferCopyLoad>& BufferCopyLoads
)
{
	if (CopyInfo.Flags & COPY_OPERATION_FLAG_ATOMIC)
	{
		size_t Size = 0;
		for (auto& CopyRegion : CopyInfo.BufferCopyRegions)
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
			return false;
		}
	}

	auto& CopyLoad = BufferCopyLoads.emplace_back();
	CopyLoad.BufferPtr = CopyInfo.DestinationBuffer;
	for (size_t CopyRegionIndex = CopyInfo.CurrentCopyRegionInline; CopyRegionIndex < CopyInfo.BufferCopyRegions.size(); CopyRegionIndex++)
	{
		VkBufferCopy& BluePrintCopyRegion = CopyInfo.BufferCopyRegions[CopyRegionIndex];

		VkBufferCopy CopyRegion{};
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
			CopyRegion.size = Budget;
			BluePrintCopyRegion.size -= Budget;
			BluePrintCopyRegion.dstOffset += Budget;
			BluePrintCopyRegion.srcOffset += Budget;
		}
		Budget -= CopyRegion.size;
		Offset += CopyRegion.size;

		memcpy(StagingBufferMappedPtr + CopyRegion.srcOffset, CopyInfo.Data->DataPtr + OriginalSrcOffset, CopyRegion.size);
		CopyLoad.CopyRegions.push_back(CopyRegion);

		if (Budget == 0) break;
	}
	
	if (CopyInfo.CurrentCopyRegionInline == CopyInfo.BufferCopyRegions.size())
	{
		CopyInfo.AllChunksProcessed->State.store(true);
		CopyInfo.Data.reset();
	}
	return true;
}

bool RENDERER::ResourceManager::ProcessImageCopyOperation(
	CopyOperationEntry& CopyInfo, 
	size_t& Budget, 
	size_t& Offset, 
	uint8_t* StagingBufferMappedPtr,
	std::vector<ImageCopyLoad>& ImageCopyLoads
)
{
	if (CopyInfo.Flags & COPY_OPERATION_FLAG_ATOMIC)
	{
		size_t Size = CopyInfo.ImageMetaData.Width * CopyInfo.ImageMetaData.Height * CopyInfo.ImageMetaData.BytesPerPixel;
		if (Size > Budget)
		{
			if (Size > PersistentStagingBufferSize)
			{
				LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Size of the data being copied is larger than the specified persistent staging buffer size!");
				throw std::runtime_error("Size of the data being copied is larger than the specified persistent staging buffer size!");
			}
			return false;
		}
	}

	auto& CopyLoad = ImageCopyLoads.emplace_back();
	CopyLoad.ImagePtr = CopyInfo.DestinationImage;
	CopyLoad.Aspect = CopyInfo.Aspect;
	for (size_t CopyRegionIndex = CopyInfo.CurrentCopyRegionInline; CopyRegionIndex < CopyInfo.ImageCopyRegions.size(); CopyRegionIndex++)
	{
		auto& BluePrintCopyRegion = CopyInfo.ImageCopyRegions[CopyRegionIndex];
		size_t BytesInRow = CopyInfo.ImageMetaData.BytesPerPixel * CopyInfo.ImageMetaData.Width;
		uint32_t RowCountInBudget = static_cast<uint32_t>(Budget / BytesInRow);
		uint32_t RemainingHeight = BluePrintCopyRegion.imageExtent.height - static_cast<uint32_t>(BluePrintCopyRegion.imageOffset.y);
		uint32_t RowCountToCopy = std::min(RemainingHeight, RowCountInBudget);

		//for now just skip this since there is no mechanism for dynamic budget
		if (RowCountToCopy == 0) continue;
		
		VkBufferImageCopy CopyRegion{};
		CopyRegion.bufferOffset = Offset;
		CopyRegion.bufferRowLength = BluePrintCopyRegion.bufferRowLength;

		CopyRegion.imageExtent.width = BluePrintCopyRegion.imageExtent.width;
		CopyRegion.imageExtent.height = RowCountToCopy;
		CopyRegion.imageExtent.depth = 1; 
		CopyRegion.imageOffset = { BluePrintCopyRegion.imageOffset.x,BluePrintCopyRegion.imageOffset.y,0 };

		CopyRegion.imageSubresource.aspectMask = BluePrintCopyRegion.imageSubresource.aspectMask;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.mipLevel = 0;

		size_t CopiedBufferCount = RowCountToCopy * BytesInRow;
		memcpy(StagingBufferMappedPtr + CopyRegion.bufferOffset, CopyInfo.Data->DataPtr + BluePrintCopyRegion.bufferOffset, CopiedBufferCount);

		if (RowCountToCopy == RemainingHeight)
		{
			CopyInfo.CurrentCopyRegionInline++;
		}
		else
		{
			BluePrintCopyRegion.imageOffset.y += RowCountToCopy;
			BluePrintCopyRegion.bufferOffset += CopiedBufferCount;
		}
		CopyLoad.CopyRegions.push_back(CopyRegion);

		Budget -= CopiedBufferCount;
		Offset += CopiedBufferCount;

		if (Budget == 0) break;
	}
	if (CopyInfo.CurrentCopyRegionInline == CopyInfo.ImageCopyRegions.size())
	{
		CopyInfo.AllChunksProcessed->State.store(true);
		CopyLoad.BarrierState = CopyInfo.OnCompleteTransition;
		CopyInfo.Data.reset();
	}
	return true;
}

void RENDERER::ResourceManager::HandleCopiesInFlight()
{
	std::vector<InFlightPayload> NewPendingQueueAcquires;
	uint64_t CurrentTimelineValue = Semaphore.GetSemaphoreCounterValue(RendererContextPtr->DeviceContext.LogicalDevice);
	while(!CopiesInFlight.empty())
	{
		auto& CurrentCopyInFlight = CopiesInFlight.front();
		if (CurrentTimelineValue >= CurrentCopyInFlight.TimelineValue)
		{
			for (auto& Payload : CurrentCopyInFlight.Payloads)
			{
				if (Payload.AllChunksProcessed->State.load())
				{
					for (auto& Token : Payload.SignalTokens)
					{
						Token->State.store(true);
					}
					NewPendingQueueAcquires.push_back(std::move(Payload));
				}
			}
			CopiesInFlight.pop_front();
		}
		else break;
	}

	if (!NewPendingQueueAcquires.empty())
	{
		std::lock_guard<std::mutex> Lock(AcquireQueueMutex);
		PendingAcquires.insert(PendingAcquires.end(), 
							   std::make_move_iterator(NewPendingQueueAcquires.begin()), 
							   std::make_move_iterator(NewPendingQueueAcquires.end())
		);
	}
}


void RENDERER::ResourceManager::HandleAsyncCopyOperations()
{
	HandleCopiesInFlight();

	if (AsyncCopyOperations.empty()) return;
	if (ShouldSortAsyncCopyInfos)
	{
		std::sort(CopyOperations.begin(), CopyOperations.end(), [&](const CopyOperationEntry& Entry0, const CopyOperationEntry& Entry1) {
			return Entry0.Priority < Entry1.Priority;
			});
	}
	ShouldSortAsyncCopyInfos = false;

	CurrentTransferContextIndex = (CurrentTransferContextIndex + 1) % AsyncTransferContexts.size();
	AsyncTransferContext& CurrentTransferContext = AsyncTransferContexts[CurrentTransferContextIndex];

	uint64_t CurrentTimelineValue = Semaphore.GetSemaphoreCounterValue(RendererContextPtr->DeviceContext.LogicalDevice);
	if (CurrentTimelineValue < CurrentTransferContext.LastSubmittedTimelineValue)
	{
		//There is no thread logic so just skip for now
		return;
	}
	CurrentTransferContext.LastSubmittedTimelineValue = CurrentTimelineValue;

	std::shared_ptr<FrameCopyLoad> CopyLoad = std::make_shared<FrameCopyLoad>();

	uint8_t* StagingBufferMappedPtr = reinterpret_cast<uint8_t*>(RingStagingBuffer.StagingBuffer.Buffer.MappedMemory);
	size_t Budget = (PersistentStagingBufferSize * 3) / AsyncTransferContexts.size();
	size_t Offset = Budget * CurrentTransferContextIndex;

	std::vector<InFlightPayload> ProcessedCopyOperationPayloads;
	for (size_t CopyInfoIndex = 0; CopyInfoIndex < AsyncCopyOperations.size(); CopyInfoIndex++)
	{
		auto& CopyInfo = AsyncCopyOperations[CopyInfoIndex];
		if (CopyInfo.BufferCopyRegions.empty() && CopyInfo.ImageCopyRegions.empty()) continue;

		bool ShouldSkip = false;
		for (auto& WaitToken : CopyInfo.WaitTokens)
		{
			if (!WaitToken->State.load())
			{
				ShouldSkip = true;
				break;
			}
		}
		if (ShouldSkip) continue;

		if (CopyInfo.DestinationBuffer)
		{
			bool Result = ProcessBufferCopyOperation(
				CopyInfo,
				Budget,
				Offset,
				StagingBufferMappedPtr,
				CopyLoad->BufferCopyLoads
			);
			if (!Result) continue;
		}
		else
		{
			bool Result = ProcessImageCopyOperation(
				CopyInfo,
				Budget,
				Offset,
				StagingBufferMappedPtr,
				CopyLoad->ImageCopyLoads
			);
			if (!Result) continue;
		}

		if (CopyInfo.AllChunksProcessed->State.load())
		{
			ShouldSortAsyncCopyInfos = true;
			InFlightPayload NewPayload{};
			NewPayload.AllChunksProcessed = CopyInfo.AllChunksProcessed;
			NewPayload.Aspect = CopyInfo.Aspect;
			NewPayload.DestinationBuffer = CopyInfo.DestinationBuffer;
			NewPayload.DestinationImage = CopyInfo.DestinationImage;
			NewPayload.OnCompleteTransition = CopyInfo.OnCompleteTransition;
			NewPayload.SignalTokens = std::move(CopyInfo.SignalTokens);
			ProcessedCopyOperationPayloads.push_back(std::move(NewPayload));
		}
		if (Budget == 0) break;
	}

	vkResetCommandPool(RendererContextPtr->DeviceContext.LogicalDevice, CurrentTransferContext.CommandPool.Handle, 0);

	RENDERER_CORE::BeginCommandBuffer(CurrentTransferContext.CommandBuffer);

	RENDERER_CORE::PipelineBarrier2 Barrier;
	for (auto& ImageCopyLoad : CopyLoad->ImageCopyLoads)
	{
		if (!ImageCopyLoad.CopyRegions.empty())
		{
			if (ImageCopyLoad.BarrierState.ImageLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				Barrier.AppendImageMemoryBarrier(
					ImageCopyLoad.ImagePtr->Image,
					ImageCopyLoad.ImagePtr->BarrierState.StageMask,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					ImageCopyLoad.ImagePtr->BarrierState.AccessMask,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					ImageCopyLoad.ImagePtr->BarrierState.ImageLayout,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					ImageCopyLoad.Aspect
				);
			}
		}
	};

	for (auto& BufferCopyLoad : CopyLoad->BufferCopyLoads)
	{
		if (!BufferCopyLoad.CopyRegions.empty())
		{
			Barrier.AppendBufferMemoryBarrier(
				BufferCopyLoad.BufferPtr->BufferObject,
				0,
				VK_WHOLE_SIZE,
				BufferCopyLoad.BufferPtr->BarrierState.StageMask,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				BufferCopyLoad.BufferPtr->BarrierState.AccessMask,
				VK_ACCESS_2_TRANSFER_WRITE_BIT
			);
		}
	};
	Barrier.ExecutePipelineBarrier(CurrentTransferContext.CommandBuffer);

	for (auto& BufferCopyLoad : CopyLoad->BufferCopyLoads)
	{
		if (!BufferCopyLoad.CopyRegions.empty())
		{
			vkCmdCopyBuffer(
				CurrentTransferContext.CommandBuffer,
				RingStagingBuffer.StagingBuffer.Buffer.BufferObject,
				BufferCopyLoad.BufferPtr->BufferObject,
				static_cast<uint32_t>(BufferCopyLoad.CopyRegions.size()),
				BufferCopyLoad.CopyRegions.data()
			);
		}
	};

	for (auto& ImageCopyLoad : CopyLoad->ImageCopyLoads)
	{
		if (!ImageCopyLoad.CopyRegions.empty())
		{
			vkCmdCopyBufferToImage(
				CurrentTransferContext.CommandBuffer,
				RingStagingBuffer.StagingBuffer.Buffer.BufferObject,
				ImageCopyLoad.ImagePtr->Image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				ImageCopyLoad.CopyRegions.size(),
				ImageCopyLoad.CopyRegions.data()
			);
		}
	};
	uint32_t TransferQueueFamily = RendererContextPtr->QueueFamilyIndices.GetQueueFamilyIndex(RENDERER_CORE::QUEUE_TYPE_TRANSFER);
	uint32_t GraphicQueueFamily = RendererContextPtr->QueueFamilyIndices.GetQueueFamilyIndex(RENDERER_CORE::QUEUE_TYPE_GRAPHICS);

	if (TransferQueueFamily != GraphicQueueFamily)
	{
		for (auto& BufferCopyLoad : ProcessedCopyOperationPayloads)
		{
			if (BufferCopyLoad.DestinationBuffer)
			{
				Barrier.AppendBufferMemoryBarrier(
					BufferCopyLoad.DestinationBuffer->BufferObject,
					0,
					VK_WHOLE_SIZE,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_PIPELINE_STAGE_2_NONE,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_ACCESS_2_NONE,
					TransferQueueFamily,
					GraphicQueueFamily
				);
			}
			else if (BufferCopyLoad.DestinationImage)
			{
				Barrier.AppendImageMemoryBarrier(
					BufferCopyLoad.DestinationImage->Image,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT,
					VK_PIPELINE_STAGE_2_NONE,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_ACCESS_2_NONE,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					BufferCopyLoad.OnCompleteTransition.ImageLayout,
					TransferQueueFamily,
					GraphicQueueFamily,
					BufferCopyLoad.Aspect
				);
			}
		}
		Barrier.ExecutePipelineBarrier(CurrentTransferContext.CommandBuffer);
	}

	RENDERER_CORE::EndCommandBuffer(CurrentTransferContext.CommandBuffer);

	TimelineSemaphoreCounter++;
	BatchInFlight NewBatch{};
	NewBatch.Context = &CurrentTransferContext;
	NewBatch.TimelineValue = TimelineSemaphoreCounter;
	NewBatch.Payloads.swap(ProcessedCopyOperationPayloads);
	NewBatch.Flag = std::make_unique<COMMON::AsyncToken>();
	CopiesInFlight.push_back(std::move(NewBatch));

	AsyncCopyOperations.erase(
		std::remove_if(AsyncCopyOperations.begin(), AsyncCopyOperations.end(), [](CopyOperationEntry& Entry) {
			return Entry.AllChunksProcessed->State == true;
			}),
		AsyncCopyOperations.end()
	);

	uint64_t SignalValues[] = { TimelineSemaphoreCounter };
	VkTimelineSemaphoreSubmitInfo TimelineSemaphoreSubmitInfo = RENDERER_CORE::TimelineSemaphoreSubmitInfo(nullptr, 0, SignalValues, 1);

	VkQueue TransferQueue = RendererContextPtr->DeviceContext.GetQueue(RENDERER_CORE::QUEUE_TYPE_TRANSFER);
	RENDERER_CORE::SubmitQueue(
		TransferQueue,
		{},
		{},
		{ CurrentTransferContext.CommandBuffer },
		{ Semaphore.Handle },
		nullptr,
		&TimelineSemaphoreSubmitInfo
	);
}



