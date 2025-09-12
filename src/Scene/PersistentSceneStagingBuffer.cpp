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
        size_t InitialCapacity = StagingBuffer.Allocator.GetCapacity() - StagingBuffer.Allocator.GetTotalFreeSpace();
        uint8_t* Space = nullptr;
        uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);

        bool IsThereAnyCopyRegion = false;
        for (uint32_t i = 0; i < CopyInfos.size(); i++)
        {
            if (!CopyInfos[i].CopyRegions.empty())
            {
                IsThereAnyCopyRegion = true;
                break;
            }
        }
        if (IsThereAnyCopyRegion)
        {
            Space = new uint8_t[InitialCapacity];
            for (uint32_t i = 0; i < static_cast<int>(BUFFER_COPY_SLOT_SIZE); i++)
            {
                for (auto& CopyRegion : CopyInfos[i].CopyRegions)
                {
                    memcpy(Space + CopyRegion.srcOffset, StagingBufferPtr + CopyRegion.srcOffset, CopyRegion.size);
                }
            }
            //memcpy(Space, StagingBuffer.Buffer.MappedMemory, InitialCapacity);
        }
        StagingBuffer.Allocator.Allocate(RequiredStagingBufferSize - StagingBuffer.Allocator.GetTotalFreeSpace());

        StagingBuffer.Buffer.Destroy(RendererContext->DeviceContext.LogicalDevice);
        RENDERER_CORE::CreateStagingBuffer(
            RendererContext->DeviceContext.PhysicalDevice,
            RendererContext->DeviceContext.LogicalDevice,
            StagingBuffer.Allocator.GetCapacity(),
            StagingBuffer.Buffer.Buffer
        );
        StagingBuffer.Buffer.Map(RendererContext->DeviceContext.LogicalDevice, 0, StagingBuffer.Allocator.GetCapacity(), 0);
        StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);

        if (Space)
        {
            //memcpy(StagingBuffer.Buffer.MappedMemory, Space, InitialCapacity);
            for (uint32_t i = 0; i < static_cast<int>(BUFFER_COPY_SLOT_SIZE); i++)
            {
                for (auto& CopyRegion : CopyInfos[i].CopyRegions)
                {
                    memcpy(StagingBufferPtr + CopyRegion.srcOffset, Space + CopyRegion.srcOffset, CopyRegion.size);
                }
            }
            delete[] Space;
        }

        for (int i = 0; i < static_cast<int>(BUFFER_COPY_SLOT_SIZE); i++)
        {
            CopyInfos[i].SourceBuffer = StagingBuffer.Buffer.Buffer.BufferObject;
        }
    }
}
