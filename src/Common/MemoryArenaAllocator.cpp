#include "MemoryArenaAllocator.hpp"
#include <algorithm>

VKCORE::VirtualArenaAllocator::VirtualArenaAllocator(size_t InitialCapacityInBytes, size_t AllocationChunkSize)
{
    Create(InitialCapacityInBytes, AllocationChunkSize);
}

void VKCORE::VirtualArenaAllocator::Create(size_t InitialCapacityInBytes, size_t AllocationChunkSize)
{
    if (AllocationChunkSize == 0) throw std::runtime_error("Allocation chunk size can't be 0!");
    Capacity = InitialCapacityInBytes;
    ChunkSize = AllocationChunkSize;
    FreeRegions.reserve(100);
    FreeRegions.push_back({ 0,InitialCapacityInBytes });
    TotalFreeSpace = InitialCapacityInBytes;
}

VKCORE::MemoryRegion VKCORE::VirtualArenaAllocator::Allocate(size_t SizeInBytes)
{
    if(SizeInBytes == 0) return { 0,0 };
    while (true)
    {
        for (size_t FreeRegionIndex = 0; FreeRegionIndex < FreeRegions.size(); FreeRegionIndex++)
        {
            auto& FreeRegion = FreeRegions[FreeRegionIndex];
            if (SizeInBytes <= FreeRegion.Size)
            {
                size_t AllocatedOffset = FreeRegion.Offset;
                if (SizeInBytes == FreeRegion.Size)
                {
                    FreeRegions.erase(FreeRegions.begin() + FreeRegionIndex);
                }
                else
                {
                    FreeRegion.Offset += SizeInBytes;
                    FreeRegion.Size -= SizeInBytes;
                }
                TotalFreeSpace -= SizeInBytes;
                return { AllocatedOffset, SizeInBytes };
            }
        }

        size_t AdditionalAllocatedSize = static_cast<size_t>(std::ceil(static_cast<float>(SizeInBytes) / static_cast<float>(ChunkSize))) * ChunkSize;
        FreeRegions.push_back({ Capacity, AdditionalAllocatedSize });
        Capacity += AdditionalAllocatedSize;
        TotalFreeSpace += AdditionalAllocatedSize;

        std::sort(FreeRegions.begin(), FreeRegions.end(), [&](auto& Region0, auto& Region1) {
            return Region0.Offset < Region1.Offset;
        });
        std::vector<MemoryRegion> MergedFreeRegions;
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
    return {0,0};
}

void VKCORE::VirtualArenaAllocator::Free(const MemoryRegion& RegionToFree)
{
    if (RegionToFree.Size == 0) throw std::runtime_error("Invalid memory region!");
    FreeRegions.push_back(RegionToFree);
    std::sort(FreeRegions.begin(), FreeRegions.end(), [&](auto& Region0, auto& Region1) {
        return Region0.Offset < Region1.Offset;
    });
    TotalFreeSpace += RegionToFree.Size;

    std::vector<MemoryRegion> MergedFreeRegions;
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

void VKCORE::VirtualArenaAllocator::Defragment(std::vector<MemoryRegion>& Regions)
{
    std::sort(Regions.begin(), Regions.end(), [&](auto& Region0, auto& Region1) {
        return Region0.Offset < Region1.Offset;
    });

    std::vector<MemoryRegion> DefragmentedRegions;
    DefragmentedRegions.reserve(Regions.size());
    size_t Offset = 0;
    for (auto& Region : Regions)
    {
        if (Region.Size == 0) continue;
        DefragmentedRegions.push_back({ Offset, Region.Size });
        Offset += Region.Size;
    }
    std::swap(DefragmentedRegions, Regions);

    FreeRegions.clear();
    FreeRegions.push_back({ Offset, Capacity - Offset });
    TotalFreeSpace = Capacity - Offset;
}

void VKCORE::VirtualArenaAllocator::Defragment(std::vector<MemoryRegion*>& Regions)
{
    std::sort(Regions.begin(), Regions.end(), [&](auto& Region0, auto& Region1) {
        return Region0->Offset < Region1->Offset;
    });

    size_t Offset = 0;
    for (auto& Region : Regions)
    {
        if (!Region || Region->Size == 0) continue;
        Region->Offset = Offset;
        Offset += Region->Size;
    }

    FreeRegions.clear();
    FreeRegions.push_back({ Offset, Capacity - Offset });
    TotalFreeSpace = Capacity - Offset;
}

double VKCORE::VirtualArenaAllocator::GetFragmentationPercent()
{
    size_t LargestFreeRegion = 0;
    for (auto& Region : FreeRegions)
    {
        LargestFreeRegion = std::max(Region.Size, LargestFreeRegion);
    }
    if (!LargestFreeRegion) return 0.0;

    return  1.0 - (static_cast<double>(LargestFreeRegion) / static_cast<double>(TotalFreeSpace));
}

void VKCORE::VirtualArenaAllocator::Reset()
{
    FreeRegions.clear();
    Capacity = this->ChunkSize;
    FreeRegions.push_back({ 0,Capacity });
    TotalFreeSpace = Capacity;
}
