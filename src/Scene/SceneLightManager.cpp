#include "SceneLightManager.hpp"
#include "Light.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

SCENE::LightManager::LightManager(RENDERER::RendererContext& RendererContext)
{
	Create(RendererContext);
}

void SCENE::LightManager::Create(RENDERER::RendererContext& RendererContext)
{
	this->RendererContext = &RendererContext;
}

void SCENE::LightManager::Destroy(VkDevice& LogicalDevice)
{
	for (auto& DynamicLightSSBO : this->DynamicLightSSBOs)
	{
		DynamicLightSSBO.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
	}
	for (auto& StaticLightSSBO : this->StaticLightSSBOs)
	{
		StaticLightSSBO.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
	}
}

void SCENE::LightManager::AppendOrUpdateLights(LightAppendOrUpdateInfo& Info)
{
	auto& InputStaticLights = Info.StaticLights;
	auto& InputDynamicLights = Info.DynamicLights;
	if (InputStaticLights.empty() && InputDynamicLights.empty()) return;

	auto& DynamicLightBuffer = DynamicLightSSBOs[Info.FrameIndex];
	auto& StaticLightBuffer = StaticLightSSBOs[Info.FrameIndex];
	auto& DynamicLightBufferAllocator = DynamicLightBuffer.Allocator;
	auto& StaticLightBufferAllocator = StaticLightBuffer.Allocator;

	auto& TargetDescriptorSet = Info.TargetDescriptorSets[Info.FrameIndex];
	auto& CurrentLightEntries = this->LightEntries[Info.FrameIndex];
	
	size_t StaticLightBufferCapacity = StaticLightBufferAllocator.GetCapacity(), 
		DynamicLightBufferCapacity = DynamicLightBufferAllocator.GetCapacity();
	size_t SizeOfLightData = sizeof(LightData);

	//Process input lights
	std::vector<std::pair<Light*,RENDERER_CORE::MemoryRegion*>> StaticLightsToUpdate;
	std::vector<std::pair<Light*, RENDERER_CORE::MemoryRegion*>> DynamicLightsToUpdate;
	for (auto& StaticLight : InputStaticLights)
	{
		auto LightIterator = CurrentLightEntries.StaticLightLights.find(StaticLight);
		if (LightIterator == CurrentLightEntries.StaticLightLights.end())
		{
			LightEntry NewEntry{};
			NewEntry.MemoryRegion = StaticLightBufferAllocator.Allocate(SizeOfLightData);
			auto [NewIterator, Inserted] = CurrentLightEntries.StaticLightLights.insert({ StaticLight,NewEntry });
			LightIterator = NewIterator;
		}
		StaticLightsToUpdate.push_back({ StaticLight, &LightIterator->second.MemoryRegion});
	}
	for (auto& DynamicLight : InputDynamicLights)
	{
		auto LightIterator = CurrentLightEntries.DynamicLights.find(DynamicLight);
		if (LightIterator == CurrentLightEntries.DynamicLights.end())
		{
			LightEntry NewEntry{};
			NewEntry.MemoryRegion = DynamicLightBufferAllocator.Allocate(SizeOfLightData);
			auto [NewIterator, Inserted] = CurrentLightEntries.DynamicLights.insert({ DynamicLight,NewEntry });
			LightIterator = NewIterator;
		}
		DynamicLightsToUpdate.push_back({ DynamicLight, &LightIterator->second.MemoryRegion });
	}

	bool IsStaticLightBufferReallocated = StaticLightBufferCapacity < StaticLightBufferAllocator.GetCapacity();
	bool IsDynamicLightBufferReallocated = DynamicLightBufferCapacity < DynamicLightBufferAllocator.GetCapacity();

	std::vector<RENDERER_CORE::DescriptorSetWriteBuffer> DescriptorWrites;
	if (IsStaticLightBufferReallocated)
	{
		StaticLightBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
		RENDERER_CORE::CreateBuffer(
			RendererContext->DeviceContext.physicalDevice,
			RendererContext->DeviceContext.logicalDevice,
			StaticLightBufferAllocator.GetCapacity(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			StaticLightBuffer.Buffer
		);
		DescriptorWrites.emplace_back(StaticLightBuffer.Buffer, StaticLightBufferAllocator.GetCapacity(), 0, TargetDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	}
	if (IsDynamicLightBufferReallocated)
	{
		DynamicLightBuffer.Buffer.Destroy(RendererContext->DeviceContext.logicalDevice);
		RENDERER_CORE::CreateBuffer(
			RendererContext->DeviceContext.physicalDevice,
			RendererContext->DeviceContext.logicalDevice,
			DynamicLightBufferAllocator.GetCapacity(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			DynamicLightBuffer.Buffer.Buffer
		);
		DynamicLightBuffer.Buffer.Map(RendererContext->DeviceContext.logicalDevice, 0, DynamicLightBufferAllocator.GetCapacity(), 0);
		DescriptorWrites.emplace_back(DynamicLightBuffer.Buffer.Buffer , DynamicLightBufferAllocator.GetCapacity(), 1, TargetDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	}
	RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.logicalDevice, DescriptorWrites, {});

	size_t StaticLightStagingBufferSize = IsStaticLightBufferReallocated ? (StaticLightBufferAllocator.GetCapacity() - StaticLightBufferAllocator.GetTotalFreeSpace()) : (StaticLightsToUpdate.size() * SizeOfLightData);
	if (!InputStaticLights.empty() && !StaticLightStagingBufferSize)
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed light updating operation, faulty staging buffer!");
		throw std::runtime_error("Faulty staging buffer!");
	}

	RENDERER_CORE::PersistentBuffer StagingBuffer{};
	RENDERER_CORE::VirtualArenaAllocator StagingBufferAllocator(StaticLightStagingBufferSize);
	uint8_t* StagingBufferPtr = nullptr;
	if (StaticLightStagingBufferSize)
	{
		RENDERER_CORE::CreateStagingBuffer(
			RendererContext->DeviceContext.physicalDevice,
			RendererContext->DeviceContext.logicalDevice,
			StaticLightStagingBufferSize,
			StagingBuffer.Buffer
		);
		StagingBuffer.Map(RendererContext->DeviceContext.logicalDevice, 0, StaticLightStagingBufferSize, 0);
		StagingBufferPtr = reinterpret_cast<uint8_t*>(StagingBuffer.MappedMemory);
		if (!StagingBufferPtr)
		{
			LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed light updating operation, faulty staging buffer!");
			throw std::runtime_error("Faulty staging buffer!");
		}
	}

	RENDERER_CORE::BufferCopyInfo StaticLightCopyInfo{};
	StaticLightCopyInfo.SourceBuffer = StagingBuffer.Buffer.BufferObject;
	StaticLightCopyInfo.DestinationBuffer = StaticLightBuffer.Buffer.BufferObject;
	if (StagingBufferPtr && StaticLightStagingBufferSize)
	{
		if (IsStaticLightBufferReallocated)
		{
			for (auto& [LightPtr, LightEntry] : CurrentLightEntries.StaticLightLights)
			{
				auto& AllocatedRegion = StagingBufferAllocator.Allocate(SizeOfLightData);
				memcpy(StagingBufferPtr + AllocatedRegion.Offset, &LightPtr->Data, AllocatedRegion.Size);

				VkBufferCopy CopyRegion{};
				CopyRegion.dstOffset = LightEntry.MemoryRegion.Offset;
				CopyRegion.size = AllocatedRegion.Size;
				CopyRegion.srcOffset = AllocatedRegion.Offset;
				StaticLightCopyInfo.CopyRegions.push_back(CopyRegion);
			}
		}
		else
		{
			for (auto& [LightPtr, AllocatedMemoryRegion] : StaticLightsToUpdate)
			{
				auto& AllocatedRegion = StagingBufferAllocator.Allocate(SizeOfLightData);
				memcpy(StagingBufferPtr + AllocatedRegion.Offset, &LightPtr->Data, AllocatedRegion.Size);

				VkBufferCopy CopyRegion{};
				CopyRegion.dstOffset = AllocatedMemoryRegion->Offset;
				CopyRegion.size = AllocatedRegion.Size;
				CopyRegion.srcOffset = AllocatedRegion.Offset;
				StaticLightCopyInfo.CopyRegions.push_back(CopyRegion);
			}
		}
	}

	if (DynamicLightBuffer.Buffer.MappedMemory)
	{
		uint8_t* DynamicLightBufferPtr = reinterpret_cast<uint8_t*>(DynamicLightBuffer.Buffer.MappedMemory);
		if (IsDynamicLightBufferReallocated)
		{
			for (auto& [LightPtr, LightEntry] : CurrentLightEntries.DynamicLights)
			{
				memcpy(DynamicLightBufferPtr + LightEntry.MemoryRegion.Offset, &LightPtr->Data, LightEntry.MemoryRegion.Size);
			}
		}
		else
		{
			for (auto& [LightPtr, AllocatedMemoryRegion] : DynamicLightsToUpdate)
			{
				memcpy(DynamicLightBufferPtr + AllocatedMemoryRegion->Offset, &LightPtr->Data, AllocatedMemoryRegion->Size);
			}
		}
	}

	if (StaticLightStagingBufferSize)
	{
		RENDERER_CORE::CopyBuffer(
			{ StaticLightCopyInfo },
			RendererContext->DeviceContext.logicalDevice,
			RendererContext->CommandPool.commandPool,
			RendererContext->DeviceContext.GraphicsQueue
		);

		StagingBuffer.Destroy(RendererContext->DeviceContext.logicalDevice);
	}
}
