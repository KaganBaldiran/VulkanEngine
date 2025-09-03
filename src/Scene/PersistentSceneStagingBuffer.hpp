#pragma once
#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Common/CommonDefinitions.hpp"

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	enum BufferCopySlots {
		INDIRECT_COPY = 0,
		DRAWMETA_COPY = 1,
		TEXTUREINDEX_COPY = 2,
		BUFFER_COPY_SLOT_SIZE
	};

	class PersistentStagingBuffer
	{
	public:
		PersistentStagingBuffer();

		void AllocateSceneStagingBuffer(
			std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos,
			size_t RequiredStagingBufferSize,
			RENDERER::RendererContext* RendererContext
		);
	    RENDERER_CORE::PersistentBufferAllocator StagingBuffer;
	};
}