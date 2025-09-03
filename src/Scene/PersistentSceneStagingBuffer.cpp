#include "PersistentSceneStagingBuffer.hpp"
#include "../Renderer/RendererContext.hpp"
#include <array>

SCENE::PersistentStagingBuffer::PersistentStagingBuffer()
{
    StagingBuffer.Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_KILOBYTE * 10);
}

void SCENE::PersistentStagingBuffer::AllocateSceneStagingBuffer(
    std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos,
    size_t RequiredStagingBufferSize,
    RENDERER::RendererContext* RendererContext
)
{
    if (RequiredStagingBufferSize > StagingBuffer.Allocator.GetTotalFreeSpace())
    {
        size_t InitialCapacity = StagingBuffer.Allocator.GetCapacity();
        uint8_t* Space = nullptr;
        if (!CopyInfos[DRAWMETA_COPY].CopyRegions.empty() || !CopyInfos[INDIRECT_COPY].CopyRegions.empty())
        {
            Space = new uint8_t[InitialCapacity];
            memcpy(Space, StagingBuffer.Buffer.MappedMemory, InitialCapacity);
        }
        StagingBuffer.Allocator.Allocate(RequiredStagingBufferSize - StagingBuffer.Allocator.GetTotalFreeSpace());

        StagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
        RENDERER_CORE::CreateStagingBuffer(
            RendererContext->DeviceContext.physicalDevice,
            RendererContext->DeviceContext.logicalDevice,
            StagingBuffer.Allocator.GetCapacity(),
            StagingBuffer.Buffer.Buffer
        );
        StagingBuffer.Buffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StagingBuffer.Allocator.GetCapacity(), 0);

        if (Space)
        {
            memcpy(StagingBuffer.Buffer.MappedMemory, Space, InitialCapacity);
            delete[] Space;
        }

        for (int i = 0; i < static_cast<int>(BUFFER_COPY_SLOT_SIZE); i++)
        {
            CopyInfos[i].SourceBuffer = StagingBuffer.Buffer.Buffer.BufferObject;
        }
    }
}
