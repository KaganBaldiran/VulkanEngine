#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>

namespace RENDERER_CORE
{
	constexpr size_t MEMORY_SIZE_MEGABYTE = 1024;
	constexpr size_t MEMORY_SIZE_HALF_MEGABYTE = 512;

	struct MemoryRegion
	{
		size_t Offset;
		size_t Size;
	};

	class VirtualArenaAllocator
	{
	public:
		VirtualArenaAllocator(size_t InitialCapacityInBytes,size_t AllocationChunkSize = MEMORY_SIZE_MEGABYTE);
		VirtualArenaAllocator() = default;
		void Create(size_t InitialCapacityInBytes = MEMORY_SIZE_MEGABYTE, size_t AllocationChunkSize = MEMORY_SIZE_MEGABYTE);

		MemoryRegion Allocate(size_t SizeInBytes);
		void Free(const MemoryRegion& RegionToFree);
		void Defragment(std::vector<MemoryRegion> &Regions);
		void Defragment(std::vector<MemoryRegion*> &Regions);
		void Reset(size_t Capacity = 0);

		double GetFragmentationPercent();
		inline size_t GetCapacity() noexcept { return Capacity; };
		inline size_t GetTotalFreeSpace() noexcept { return TotalFreeSpace; };
	private:
		size_t Capacity = 0,ChunkSize = MEMORY_SIZE_MEGABYTE,TotalFreeSpace = 0;
		std::vector<MemoryRegion> FreeRegions;
	};
}
