#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>

namespace RENDERER_CORE
{
	constexpr size_t MEMORY_SIZE_KILOBYTE = 1024;
	constexpr size_t MEMORY_SIZE_HALF_KILOBYTE = 512;

	struct MemoryRegion
	{
		//Data that represents the region. Can be directly used with no further processing.

		//Offset with the start padding (OriginalOffset + StartPadding).
		size_t Offset = 0;
		//Size that consists of (RequestedSize + EndPadding).
		size_t Size = 0;

		//Data that allocator will use to free the region.
		
		//Offset without the start padding.
		size_t OffsetWithoutPadding = 0;
		//Size that consists of (StartPadding + RequestedSize + EndPadding).
		size_t TotalConsumedSize = 0;
		//The requested alignment. 
		size_t Alignment = 0;
	};

	struct AllocatableMemoryRegion
	{
		size_t Offset;
		size_t Size;
	};

	size_t AlignUp(size_t Size, size_t Alignment);

	class VirtualArenaAllocator
	{
	public:
		VirtualArenaAllocator(size_t InitialCapacityInBytes,size_t AllocationChunkSize = MEMORY_SIZE_KILOBYTE);
		VirtualArenaAllocator() = default;
		void Create(size_t InitialCapacityInBytes = MEMORY_SIZE_KILOBYTE, size_t AllocationChunkSize = MEMORY_SIZE_KILOBYTE);

		MemoryRegion Suballocate(size_t SizeInBytes, size_t Alignment = 1,bool AutoAllocate = true);
		void Allocate(size_t SizeInBytes);
		void Free(const MemoryRegion& RegionToFree);
		void Defragment(std::vector<MemoryRegion> &Regions);
		void Defragment(std::vector<MemoryRegion*> &Regions);
		void Reset(size_t Capacity = 0);

		double GetFragmentationPercent();
		inline size_t GetCapacity() const noexcept { return Capacity; };
		inline size_t GetTotalFreeSpace() const noexcept { return TotalFreeSpace; };
		inline size_t GetUsedSpace() const noexcept { return Capacity - TotalFreeSpace; };
		inline const std::vector<AllocatableMemoryRegion>& GetFreeRegions() { return FreeRegions; };
	private:
		size_t Capacity = 0,ChunkSize = MEMORY_SIZE_KILOBYTE,TotalFreeSpace = 0;
		std::vector<AllocatableMemoryRegion> FreeRegions;
	};
}
