#pragma once
#include "MaterialManager.hpp"
#include "MeshManager.hpp"

#include "Core/VulkanSynchoronization.hpp"
#include "Core/VulkanDevice.hpp"
#include "Core/VulkanCommandPool.hpp"
#include "../Scene/PersistentSceneStagingBuffer.hpp"

#include "../Common/StableVector.hpp"
#include "../Common/DestructionQueue.hpp"
#include "../Common/AsyncToken.hpp"

#include <thread>
#include <mutex>

namespace SCENE
{
	class SceneMeshManager;
	class Scene;
}

namespace RENDERER
{
	class DeferredRenderPipeline;
	class Renderer;
	class FrameGraph;

	struct DataBlock
	{
		uint8_t* DataPtr = nullptr;
		size_t SizeInBytes = 0;

		std::function<void()> Deleter;
		~DataBlock(){ if (Deleter) Deleter();}

		DataBlock(const DataBlock&) = delete;
		DataBlock& operator=(const DataBlock&) = delete;
		DataBlock() = default;
	};

	enum CopyOperationFlagBit
	{
		COPY_OPERATION_FLAG_NONE = 0,
		COPY_OPERATION_FLAG_ATOMIC = 1
	};

	struct CopyOperationEntry
	{
		std::vector<VkBufferCopy> BufferCopyRegions;
		RENDERER_CORE::Buffer* DestinationBuffer = nullptr;

		RENDERER_CORE::ImageData* DestinationImage = nullptr;
		VkImageAspectFlags Aspect;
		std::vector<VkBufferImageCopy> ImageCopyRegions;
		RENDERER_CORE::ImageMetaData ImageMetaData;

		RENDERER_CORE::BarrierState OnCompleteTransition;
		std::shared_ptr<DataBlock> Data;
		size_t CurrentCopyRegionInline = 0;
		uint32_t Priority = 0;
		std::shared_ptr<COMMON::AsyncToken> AllChunksProcessed;
		CopyOperationFlagBit Flags = COPY_OPERATION_FLAG_NONE;

		std::vector<std::shared_ptr<COMMON::AsyncToken>> WaitTokens;
		std::vector<std::shared_ptr<COMMON::AsyncToken>> SignalTokens;
	};
	using CopyOperationList = std::vector<CopyOperationEntry>;

	class ResourceManager : COMMON::Destructible
	{
		friend class SCENE::SceneMeshManager;
		friend class SCENE::Scene;
		friend class DeferredRenderPipeline;
		friend class Renderer;
	public:
		ResourceManager(RENDERER::RendererContext& RendererContext);
		ResourceManager() = default;
		void Create(RENDERER::RendererContext &RendererContext);
		void Destroy() override;

		void AppendModelImportTask(ModelImportInfo ImportInfo);
		void AppendTextureImportTask(TextureImportInfo ImportInfo);
		void SubmitTextureImports();
		void SubmitModelImports();
		void WaitModelImportsIdle();
		void WaitTextureImportsIdle();

		void RequestBufferCopyOperation(
			bool Asynchronous,
			const std::vector<VkBufferCopy>& CopyRegions,
			RENDERER_CORE::Buffer* DestinationBuffer,
			const std::shared_ptr<DataBlock> &Data,
			uint32_t Priority = 0,
			CopyOperationFlagBit Flags = COPY_OPERATION_FLAG_NONE,
			RENDERER_CORE::BarrierState OnCompleteTransition = RENDERER_CORE::BarrierState(),
			std::shared_ptr<COMMON::AsyncToken>* WaitTokens = nullptr,
			uint32_t WaitTokenCount = 0,
			std::shared_ptr<COMMON::AsyncToken>* SignalTokens = nullptr,
			uint32_t SignalTokenCount = 0
		);

		void RequestImageCopyOperation(
			bool Asynchronous,
			const std::vector<VkBufferImageCopy>& CopyRegions,
			RENDERER_CORE::ImageData* DestinationImage,
			const std::shared_ptr<DataBlock>& Data,
			RENDERER_CORE::ImageMetaData ImageMetaData,
			VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
			uint32_t Priority = 0,
			CopyOperationFlagBit Flags = COPY_OPERATION_FLAG_NONE,
			RENDERER_CORE::BarrierState OnCompleteTransition = RENDERER_CORE::BarrierState(),
			std::shared_ptr<COMMON::AsyncToken>* WaitTokens = nullptr,
			uint32_t WaitTokenCount = 0,
			std::shared_ptr<COMMON::AsyncToken>* SignalTokens = nullptr,
			uint32_t SignalTokenCount = 0
		);
		
		void QueueTransientCopyOperations(
			VkCommandBuffer& CommandBuffer,
			size_t FrameIndex,
			FrameGraph &FrameGraph
		);

		void HandleAsyncCopyOperations();

		MeshManager MeshManager;
		TextureManager TextureManager;
	private:

		struct BufferCopyLoad
		{
			std::vector<VkBufferCopy> CopyRegions;
			RENDERER_CORE::Buffer* BufferPtr = nullptr;
		};

		struct ImageCopyLoad
		{
			std::vector<VkBufferImageCopy> CopyRegions;
			RENDERER_CORE::ImageData* ImagePtr = nullptr;
			RENDERER_CORE::BarrierState BarrierState;
			VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		};

		struct FrameCopyLoad
		{
			std::vector<BufferCopyLoad> BufferCopyLoads;
			std::vector<ImageCopyLoad> ImageCopyLoads;
		};

		struct AsyncTransferContext
		{
			RENDERER_CORE::CommandPool CommandPool;
			VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
			
			uint64_t LastSubmittedTimelineValue = 0;
		};

		struct InFlightPayload
		{
			RENDERER_CORE::Buffer* DestinationBuffer = nullptr;
			RENDERER_CORE::ImageData* DestinationImage = nullptr;
			VkImageAspectFlags Aspect;
			RENDERER_CORE::BarrierState OnCompleteTransition;
			std::shared_ptr<COMMON::AsyncToken> AllChunksProcessed;
			std::vector<std::shared_ptr<COMMON::AsyncToken>> SignalTokens;
		};

		struct BatchInFlight
		{
			uint64_t TimelineValue = 0;
			std::shared_ptr<COMMON::AsyncToken> Flag;
			AsyncTransferContext* Context = nullptr;
			
			std::vector<InFlightPayload> Payloads;
		};

		void LoadMemoryChunks(
			uint32_t FrameIndex,
			std::vector<BufferCopyLoad>& BufferCopyLoads,
			std::vector<ImageCopyLoad>& ImageCopyLoads
		);
		bool ProcessBufferCopyOperation(
			CopyOperationEntry& CopyInfo,
			size_t& Budget,
			size_t& Offset,
			uint8_t* StagingBufferMappedPtr,
			std::vector<BufferCopyLoad>& BufferCopyLoads
		);
		bool ProcessImageCopyOperation(
			CopyOperationEntry& CopyInfo,
			size_t& Budget,
			size_t& Offset,
			uint8_t* StagingBufferMappedPtr,
			std::vector<ImageCopyLoad>& ImageCopyLoads
		);
		bool ShouldSortCopyInfos = true;

		void RecordAcquireBarriers(
			VkCommandBuffer DestinationCommandBuffer,
			const std::vector<InFlightPayload>& CompletedTransfers
		);
		void HandleCopiesInFlight();

		std::array<SCENE::PersistentStagingBuffer,MAX_FRAMES_IN_FLIGHT> TransientStagingBuffers;
		std::vector<CopyOperationEntry> CopyOperations;
		
		std::deque<BatchInFlight> CopiesInFlight;
		SCENE::PersistentStagingBuffer RingStagingBuffer;
		RENDERER_CORE::PipelineBarrier2 InterQueueBarrier;

		std::mutex AcquireQueueMutex;
		std::vector<InFlightPayload> PendingAcquires;
		
		std::vector<uint32_t> FreeContexts;
		std::vector<CopyOperationEntry> AsyncCopyOperations;
		std::array<AsyncTransferContext,5> AsyncTransferContexts;
		uint32_t CurrentTransferContextIndex = 0;
		uint64_t TimelineSemaphoreCounter = 0;

		RENDERER_CORE::TimelineSemaphore Semaphore;
		bool ShouldSortAsyncCopyInfos = true;

		RENDERER::RendererContext* RendererContextPtr = nullptr;
	};
}