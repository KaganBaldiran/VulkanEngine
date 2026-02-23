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
		size_t Offset;
		size_t Size;
	};

	class VirtualArenaAllocator
	{
	public:
		VirtualArenaAllocator(size_t InitialCapacityInBytes,size_t AllocationChunkSize = MEMORY_SIZE_KILOBYTE);
		VirtualArenaAllocator() = default;
		void Create(size_t InitialCapacityInBytes = MEMORY_SIZE_KILOBYTE, size_t AllocationChunkSize = MEMORY_SIZE_KILOBYTE);

		MemoryRegion Suballocate(size_t SizeInBytes,bool AutoAllocate = true);
		void Allocate(size_t SizeInBytes);
		void Free(const MemoryRegion& RegionToFree);
		void Defragment(std::vector<MemoryRegion> &Regions);
		void Defragment(std::vector<MemoryRegion*> &Regions);
		void Reset(size_t Capacity = 0);

		double GetFragmentationPercent();
		inline size_t GetCapacity() const noexcept { return Capacity; };
		inline size_t GetTotalFreeSpace() const noexcept { return TotalFreeSpace; };
		inline size_t GetUsedSpace() const noexcept { return Capacity - TotalFreeSpace; };
		inline const std::vector<MemoryRegion>& GetFreeRegions() { return FreeRegions; };
	private:
		size_t Capacity = 0,ChunkSize = MEMORY_SIZE_KILOBYTE,TotalFreeSpace = 0;
		std::vector<MemoryRegion> FreeRegions;
	};
}
