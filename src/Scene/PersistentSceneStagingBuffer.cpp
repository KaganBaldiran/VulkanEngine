#include "PersistentSceneStagingBuffer.hpp"
#include "../Renderer/RendererContext.hpp"
#include <array>

SCENE::PersistentStagingBuffer::PersistentStagingBuffer()
{
    StagingBuffer.Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_KILOBYTE * 10);
}

void SCENE::PersistentStagingBuffer::AllocateSceneStagingBuffer(
    size_t RequiredStagingBufferSize,
    RENDERER::RendererContext* RendererContext
)
{
    if (RequiredStagingBufferSize > StagingBuffer.Allocator.GetTotalFreeSpace())
    {
        size_t InitialCapacity = StagingBuffer.Allocator.GetCapacity() - StagingBuffer.Allocator.GetTotalFreeSpace();
        uint8_t* Space = nullptr;
        uint8_t* StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.Buffer.MappedMemory);

        std::vector<RENDERER_CORE::MemoryRegion> Regions;
        if (InitialCapacity)
        {
            auto& FreeRegions = StagingBuffer.Allocator.GetFreeRegions();
            Regions.reserve(FreeRegions.size());

            Space = new uint8_t[InitialCapacity]();
            size_t Offset = 0, TempBufferOffset = 0;
            if (FreeRegions.empty())
            {
                Regions.push_back({ 0,StagingBuffer.Allocator.GetCapacity() });
            }
            else
            {
                for (size_t i = 0; i < FreeRegions.size(); i++)
                {
                    const auto& FreeRegion = FreeRegions[i];
                    if (Offset != FreeRegion.Offset)
                    {
                        size_t Size = FreeRegion.Offset - Offset;
                        Regions.push_back({ Offset,Size });
                    }
                    Offset = FreeRegion.Offset + FreeRegion.Size;
                }
                if (Offset < StagingBuffer.Allocator.GetCapacity())
                {
                    Regions.push_back({ Offset,StagingBuffer.Allocator.GetCapacity() - Offset });
                }
            }

            size_t TempOffset = 0;
            for (auto& Region : Regions)
            {
                memcpy(Space + TempOffset, StagingBufferPtr + Region.Offset, Region.Size);
                TempOffset += Region.Size;
            }
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
            size_t TempOffset = 0;
            for (auto& Region : Regions)
            {
                memcpy(StagingBufferPtr + Region.Offset,Space + TempOffset, Region.Size);
                TempOffset += Region.Size;
            }

            delete[] Space;
        }
    }
}
