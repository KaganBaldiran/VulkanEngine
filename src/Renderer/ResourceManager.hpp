#pragma once
#include "MaterialManager.hpp"
#include "MeshManager.hpp"

#include "Core/VulkanSynchoronization.hpp"
#include "../Scene/PersistentSceneStagingBuffer.hpp"
#include "../Common/StableVector.hpp"
#include "../Common/DestructionQueue.hpp"

namespace SCENE
{
	class SceneMeshManager;
	class Scene;
}

namespace RENDERER
{
	class DeferredRenderPipeline;
	class Renderer;

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

		struct CopyOperationEntry
		{
			RENDERER_CORE::BufferCopyInfo CopyInfo;
			RENDERER_CORE::MemoryBufferBarrierState BufferState;
		};
	private:
		std::array<SCENE::PersistentStagingBuffer,MAX_FRAMES_IN_FLIGHT> StagingBuffers;

		using CopyOperationList = COMMON::StableVector<CopyOperationEntry>;
		std::array<CopyOperationList,MAX_FRAMES_IN_FLIGHT> CopyInfos;
		std::array<std::vector<uint32_t>,MAX_FRAMES_IN_FLIGHT> PendingCopyOperations;

		//Creates the requested list or/and returns index to the respective copy info list
		size_t RequestCopyOperation(
			VkBuffer DestinationBuffer,
			uint32_t FrameIndex,
			VkPipelineStageFlags2 SrcStageMask,
			VkPipelineStageFlags2 DstStageMask,
			VkAccessFlags2 SrcAccessMask,
			VkAccessFlags2 DstAccessMask
		);
		RENDERER::ResourceManager::CopyOperationEntry* GetCopyOperationEntry(const size_t& Index, const uint32_t& FrameIndex);
		void SetCopyOperationDirty(size_t Index, uint32_t FrameIndex);
		void HandleCopyOperations(
			VkCommandBuffer& CommandBuffer,
			size_t FrameIndex,
			RENDERER_CORE::PipelineBarrier2& PipelineBarrier2
		);

		MeshManager MeshManager;
		TextureManager TextureManager;
	   
		RENDERER::RendererContext* RendererContextPtr = nullptr;
	};
}