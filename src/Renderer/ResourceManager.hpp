#pragma once
#include "MaterialManager.hpp"
#include "MeshManager.hpp"

#include "Core/VulkanSynchoronization.hpp"
#include "Core/VulkanDevice.hpp"
#include "../Scene/PersistentSceneStagingBuffer.hpp"

#include "../Common/StableVector.hpp"
#include "../Common/DestructionQueue.hpp"
#include "../Common/AsyncToken.hpp"

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

		~DataBlock()
		{
			if (Deleter)
			{
				Deleter();
			}
		}

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
		RENDERER_CORE::QueueType QueueType;

		std::vector<VkBufferCopy> CopyRegions;
		VkBuffer SourceBuffer = VK_NULL_HANDLE;
		RENDERER_CORE::Buffer* DestinationBuffer = nullptr;

		std::shared_ptr<DataBlock> Data;
		size_t CurrentCopyRegionInline = 0;
		uint32_t Priority = 0;
		uint64_t TargetSemaphoreValue = 0;
		std::shared_ptr<COMMON::AsyncToken> State;
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

		void RequestCopyOperation(
			const std::vector<VkBufferCopy>& CopyRegions,
			RENDERER_CORE::QueueType QueueType,
			RENDERER_CORE::Buffer* DestinationBuffer,
			const std::shared_ptr<DataBlock> &Data,
			uint32_t Priority = 0,
			CopyOperationFlagBit Flags = COPY_OPERATION_FLAG_NONE,
			std::shared_ptr<COMMON::AsyncToken>* WaitTokens = nullptr,
			uint32_t WaitTokenCount = 0,
			std::shared_ptr<COMMON::AsyncToken>* SignalTokens = nullptr,
			uint32_t SignalTokenCount = 0
		);
		
		void HandleCopyOperations(
			VkCommandBuffer& CommandBuffer,
			size_t FrameIndex,
			RENDERER_CORE::PipelineBarrier2& PipelineBarrier2
		);

		void QueueCopyOperations(
			VkCommandBuffer& CommandBuffer,
			size_t FrameIndex,
			FrameGraph &FrameGraph
		);

		MeshManager MeshManager;
		TextureManager TextureManager;
	private:
		void LoadMemoryChunks(VkCommandBuffer CommandBuffer,uint32_t FrameIndex);
		bool ShouldSortCopyInfos = true;

		std::array<SCENE::PersistentStagingBuffer, MAX_FRAMES_IN_FLIGHT> StagingBuffers;
		CopyOperationList CopyOperations;
   
		RENDERER::RendererContext* RendererContextPtr = nullptr;
	};
}