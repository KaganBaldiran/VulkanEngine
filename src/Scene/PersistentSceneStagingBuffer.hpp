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
		TRANSFORMATION_MATRIX_COPY = 3,
		BUFFER_COPY_SLOT_SIZE
	};

	class PersistentStagingBuffer
	{
	public:
		PersistentStagingBuffer();

		void AllocateSceneStagingBuffer(
			size_t RequiredStagingBufferSize,
			RENDERER::RendererContext* RendererContext
		);
	    RENDERER_CORE::BufferAllocator StagingBuffer;
	};
}