#include "MemoryArenaAllocator.hpp"
#include <algorithm>

RENDERER_CORE::VirtualArenaAllocator::VirtualArenaAllocator(size_t InitialCapacityInBytes, size_t AllocationChunkSize)
{
    Create(InitialCapacityInBytes, AllocationChunkSize);
}

void RENDERER_CORE::VirtualArenaAllocator::Create(size_t InitialCapacityInBytes, size_t AllocationChunkSize)
{
    if (AllocationChunkSize == 0) throw std::runtime_error("Allocation chunk size can't be 0!");
    Capacity = InitialCapacityInBytes;
    ChunkSize = AllocationChunkSize;
    FreeRegions.reserve(100);
    FreeRegions.push_back({ 0,InitialCapacityInBytes });
    TotalFreeSpace = InitialCapacityInBytes;
}

RENDERER_CORE::MemoryRegion RENDERER_CORE::VirtualArenaAllocator::Suballocate(size_t SizeInBytes,size_t Alignment, bool AutoAllocate)
{
    if(SizeInBytes == 0) return { 0, 0, 0, 0, 0 };
    size_t AlignedSize = AlignUp(SizeInBytes, Alignment);
    while (true)
    {
        for (size_t FreeRegionIndex = 0; FreeRegionIndex < FreeRegions.size(); FreeRegionIndex++)
        {
            auto& FreeRegion = FreeRegions[FreeRegionIndex];
            size_t AllocatedOffset = FreeRegion.Offset;
            size_t StartPadding = AlignUp(AllocatedOffset, Alignment) - AllocatedOffset;
            size_t TotalRequiredSize = AlignedSize + StartPadding;
            if (TotalRequiredSize <= FreeRegion.Size)
            {
                if (TotalRequiredSize == FreeRegion.Size)
                {
                    FreeRegions.erase(FreeRegions.begin() + FreeRegionIndex);
                }
                else
                {
                    FreeRegion.Offset += TotalRequiredSize;
                    FreeRegion.Size -= TotalRequiredSize;
                }
                TotalFreeSpace -= TotalRequiredSize;
                return { AllocatedOffset + StartPadding,AlignedSize,AllocatedOffset,TotalRequiredSize,Alignment };
            }
        }
        if (AutoAllocate) Allocate(AlignedSize + Alignment - 1);
        else break;
    }
    return { 0, 0, 0, 0, 0 };
}

void RENDERER_CORE::VirtualArenaAllocator::Allocate(size_t SizeInBytes)
{
    //size_t AdditionalAllocatedSize = AlignUp(SizeInBytes, ChunkSize);
    size_t AdditionalAllocatedSize = static_cast<size_t>(std::ceil(static_cast<float>(SizeInBytes) / static_cast<float>(ChunkSize))) * ChunkSize;
    FreeRegions.push_back({ Capacity,AdditionalAllocatedSize });
    Capacity += AdditionalAllocatedSize;
    TotalFreeSpace += AdditionalAllocatedSize;

    std::sort(FreeRegions.begin(), FreeRegions.end(), [&](auto& Region0, auto& Region1) {
        return Region0.Offset < Region1.Offset;
    });
    std::vector<AllocatableMemoryRegion> MergedFreeRegions;
    MergedFreeRegions.reserve(FreeRegions.size());
    for (auto& FreeRegion : FreeRegions)
    {
        if (!MergedFreeRegions.empty() && (MergedFreeRegions.back().Offset + MergedFreeRegions.back().Size) == FreeRegion.Offset)
        {
            MergedFreeRegions.back().Size += FreeRegion.Size;
        }
        else
        {
            MergedFreeRegions.push_back(FreeRegion);
        }
    }
    std::swap(FreeRegions, MergedFreeRegions);
}

void RENDERER_CORE::VirtualArenaAllocator::Free(const MemoryRegion& RegionToFree)
{
    if (RegionToFree.Size == 0 || (RegionToFree.OffsetWithoutPadding + RegionToFree.TotalConsumedSize) > Capacity) throw std::runtime_error("Invalid memory region!");
    FreeRegions.push_back({ RegionToFree.OffsetWithoutPadding,RegionToFree.TotalConsumedSize});
    std::sort(FreeRegions.begin(), FreeRegions.end(), [&](auto& Region0, auto& Region1) {
        return Region0.Offset < Region1.Offset;
    });
    TotalFreeSpace += RegionToFree.TotalConsumedSize;

    std::vector<AllocatableMemoryRegion> MergedFreeRegions;
    for (auto& FreeRegion : FreeRegions)
    {
        if (!MergedFreeRegions.empty() && (MergedFreeRegions.back().Offset + MergedFreeRegions.back().Size) == FreeRegion.Offset)
        {
            MergedFreeRegions.back().Size += FreeRegion.Size;
        }
        else
        {
            MergedFreeRegions.push_back(FreeRegion);
        }
    }
    std::swap(FreeRegions, MergedFreeRegions);
}

void RENDERER_CORE::VirtualArenaAllocator::Defragment(std::vector<MemoryRegion>& Regions)
{
    std::sort(Regions.begin(), Regions.end(), [&](auto& Region0, auto& Region1) {
        return Region0.OffsetWithoutPadding < Region1.OffsetWithoutPadding;
    });

    std::vector<MemoryRegion> DefragmentedRegions;
    DefragmentedRegions.reserve(Regions.size());
    size_t Offset = 0;
    for (auto& Region : Regions)
    {
        if (Region.Size == 0 || Region.TotalConsumedSize == 0) continue;

        size_t AlignedOffset = AlignUp(Offset, Region.Alignment);
        size_t StartPadding = AlignedOffset - Offset;
        size_t TotalConsumedSize = Region.Size + StartPadding;

        DefragmentedRegions.push_back({ AlignedOffset, Region.Size,Offset, TotalConsumedSize ,Region.Alignment });
        Offset += TotalConsumedSize;
    }
    std::swap(DefragmentedRegions, Regions);

    FreeRegions.clear();
    FreeRegions.push_back({ Offset, Capacity - Offset });
    TotalFreeSpace = Capacity - Offset;
}

void RENDERER_CORE::VirtualArenaAllocator::Defragment(std::vector<MemoryRegion*>& Regions)
{
    std::sort(Regions.begin(), Regions.end(), [&](auto& Region0, auto& Region1) {
        return Region0->OffsetWithoutPadding < Region1->OffsetWithoutPadding;
        });

    size_t Offset = 0;
    for (auto& Region : Regions)
    {
        if (!Region || Region->Size == 0 || Region->TotalConsumedSize == 0) continue;

        size_t AlignedOffset = AlignUp(Offset, Region->Alignment);
        size_t StartPadding = AlignedOffset - Offset;
        size_t TotalConsumedSize = Region->Size + StartPadding;

        Region->Offset = AlignedOffset;
        Region->OffsetWithoutPadding = Offset;
        Region->TotalConsumedSize = TotalConsumedSize;

        Offset += TotalConsumedSize;
    }

    FreeRegions.clear();
    FreeRegions.push_back({ Offset, Capacity - Offset });
    TotalFreeSpace = Capacity - Offset;
}

double RENDERER_CORE::VirtualArenaAllocator::GetFragmentationPercent()
{
    size_t LargestFreeRegion = 0;
    for (auto& Region : FreeRegions)
    {
        LargestFreeRegion = std::max(Region.Size, LargestFreeRegion);
    }
    if (!LargestFreeRegion) return 0.0;

    return  1.0 - (static_cast<double>(LargestFreeRegion) / static_cast<double>(TotalFreeSpace));
}

void RENDERER_CORE::VirtualArenaAllocator::Reset(size_t Capacity)
{
    FreeRegions.clear();
    this->Capacity = Capacity;
    FreeRegions.push_back({ 0,Capacity });
    TotalFreeSpace = Capacity;
}

size_t RENDERER_CORE::AlignUp(size_t Size, size_t Alignment)
{
    if (Alignment == 1) return Size;
    return ((Size + Alignment - 1) / Alignment) * Alignment;
}
