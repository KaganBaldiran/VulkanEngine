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
	CommandPool.Create(
		RendererContext.QueueFamilyIndices.TransferFamily.value(),
		RendererContext.DeviceContext.LogicalDevice
	);

	CommandBuffers.resize(5);
	FreeCommandBuffers.reserve(5);
	RENDERER_CORE::AllocateCommandBuffers(
		CommandPool.Handle,
		RendererContextPtr->DeviceContext.LogicalDevice,
		CommandBuffers.data(),
		CommandBuffers.size()
	);
	for (auto& StagingBuffer : TransientStagingBuffers)
	{
		StagingBuffer.AllocateSceneStagingBuffer(PersistentStagingBufferSize, RendererContextPtr);
	}
	//RingStagingBuffer.AllocateSceneStagingBuffer(PersistentStagingBufferSize * 3, RendererContextPtr);

	IsDestroyed = false;
	DestructionPriority = 2;
	COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::ResourceManager::Destroy()
{
	if (IsDestroyed) return;
	CommandPool.Destroy(RendererContextPtr->DeviceContext.LogicalDevice);
	
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
	NewEntry.BufferCopyRegions = CopyRegions;
	NewEntry.SignalTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(SignalTokens, SignalTokens + SignalTokenCount));
	NewEntry.WaitTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(WaitTokens, WaitTokens + WaitTokenCount));
	NewEntry.State = std::make_shared<COMMON::AsyncToken>();
	this->CopyOperations.push_back(std::move(NewEntry));
	ShouldSortCopyInfos = true;
}

void RENDERER::ResourceManager::RequestImageCopyOperation(
	const std::vector<VkBufferImageCopy>& CopyRegions,
	RENDERER_CORE::QueueType QueueType,
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
	NewEntry.QueueType = QueueType;
	NewEntry.ImageCopyRegions = CopyRegions;
	NewEntry.Aspect = Aspect;
	NewEntry.OnCompleteTransition = std::move(OnCompleteTransition);
	NewEntry.ImageMetaData = std::move(ImageMetaData);
	NewEntry.SignalTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(SignalTokens, SignalTokens + SignalTokenCount));
	NewEntry.WaitTokens = std::move(std::vector<std::shared_ptr<COMMON::AsyncToken>>(WaitTokens, WaitTokens + WaitTokenCount));
	NewEntry.State = std::make_shared<COMMON::AsyncToken>();
	this->CopyOperations.push_back(std::move(NewEntry));
	ShouldSortCopyInfos = true;
}

void RENDERER::ResourceManager::QueueCopyOperations(VkCommandBuffer& CommandBuffer, size_t FrameIndex, FrameGraph& FrameGraph)
{
	if (CopyOperations.empty()) return;

	std::shared_ptr<FrameCopyLoad> CopyLoad = std::make_shared<FrameCopyLoad>();
	LoadMemoryChunks(FrameIndex, CopyLoad->BufferCopyLoads, CopyLoad->ImageCopyLoads);
	auto& StagingBuffer = TransientStagingBuffers[FrameIndex].StagingBuffer;

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

		[this,CopyLoad,&StagingBuffer](VkCommandBuffer CommandBuffer,uint32_t CurrentFrame) {
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
		if (Budget == 0) break;
	}

	CopyOperations.erase(
		std::remove_if(CopyOperations.begin(), CopyOperations.end(),[](CopyOperationEntry& Entry) {
			return Entry.State->State == true;
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
		CopyInfo.State->State.store(true);

		for (auto& SignalToken : CopyInfo.SignalTokens)
		{
			SignalToken->State.store(true);
		}

		CopyInfo.Data.reset();
		ShouldSortCopyInfos = true;
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
		CopyInfo.State->State.store(true);

		for (auto& SignalToken : CopyInfo.SignalTokens)
		{
			SignalToken->State.store(true);
		}

		CopyLoad.BarrierState = CopyInfo.OnCompleteTransition;
		CopyInfo.Data.reset();
		ShouldSortCopyInfos = true;
	}
	return true;
}

void RENDERER::ResourceManager::HandleCopiesInFlight()
{
	uint64_t CurrentTimelineValue = Semaphore.GetSemaphoreCounterValue(RendererContextPtr->DeviceContext.LogicalDevice);
	while(!CopiesInFlight.empty())
	{
		auto& CurrentCopyInFlight = CopiesInFlight.front();
		if (CurrentTimelineValue >= CurrentCopyInFlight.TimelineValue)
		{
			CurrentCopyInFlight.Flag->State.store(true);


			CopiesInFlight.pop_front();
		}
		else break;
	}
}



