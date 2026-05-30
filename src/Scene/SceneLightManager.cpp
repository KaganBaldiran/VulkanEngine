#include "SceneLightManager.hpp"
#include "Light.hpp"

#include "../Renderer/RendererContext.hpp"
#include "../Renderer/ResourceManager.hpp"
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
		RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, DynamicLightSSBO.Buffer);
	}
	for (auto& StaticLightSSBO : this->StaticLightSSBOs)
	{
		RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, StaticLightSSBO.Buffer);
	}
}

void SCENE::LightManager::AppendOrUpdateLights(
		std::vector<Light*> &StaticLights,
		std::vector<Light*> &DynamicLights,
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> &TargetDescriptorSets,
		uint32_t FrameIndex,
	    RENDERER::ResourceManager* ResourceManagerPtr
)
{
	auto& InputStaticLights = StaticLights;
	auto& InputDynamicLights = DynamicLights;
	if (InputStaticLights.empty() && InputDynamicLights.empty()) return;

	auto& DynamicLightBuffer = DynamicLightSSBOs[FrameIndex];
	auto& StaticLightBuffer = StaticLightSSBOs[FrameIndex];
	auto& DynamicLightBufferAllocator = DynamicLightBuffer.Allocator;
	auto& StaticLightBufferAllocator = StaticLightBuffer.Allocator;

	auto& TargetDescriptorSet = TargetDescriptorSets[FrameIndex];
	auto& CurrentLightEntries = this->LightEntries[FrameIndex];
	
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
			NewEntry.MemoryRegion = StaticLightBufferAllocator.Suballocate(SizeOfLightData);
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
			NewEntry.MemoryRegion = DynamicLightBufferAllocator.Suballocate(SizeOfLightData);
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
		RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, StaticLightBuffer.Buffer);
		RENDERER_CORE::CreateBuffer(
			RendererContext->DeviceContext.PhysicalDevice,
			RendererContext->DeviceContext.LogicalDevice,
			StaticLightBufferAllocator.GetCapacity(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			StaticLightBuffer.Buffer
		);
		DescriptorWrites.emplace_back(StaticLightBuffer.Buffer, StaticLightBufferAllocator.GetCapacity(), 0, TargetDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	}
	if (IsDynamicLightBufferReallocated)
	{
		RENDERER_CORE::DestroyBuffer(RendererContext->DeviceContext.LogicalDevice, DynamicLightBuffer.Buffer);
		RENDERER_CORE::CreateBuffer(
			RendererContext->DeviceContext.PhysicalDevice,
			RendererContext->DeviceContext.LogicalDevice,
			DynamicLightBufferAllocator.GetCapacity(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			DynamicLightBuffer.Buffer
		);
		RENDERER_CORE::MapBuffer(DynamicLightBuffer.Buffer, RendererContext->DeviceContext.LogicalDevice, 0, DynamicLightBufferAllocator.GetCapacity(), 0);
		DescriptorWrites.emplace_back(DynamicLightBuffer.Buffer , DynamicLightBufferAllocator.GetCapacity(), 1, TargetDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	}
	RENDERER_CORE::WriteDescriptorSets(RendererContext->DeviceContext.LogicalDevice, DescriptorWrites, {});

	size_t StaticLightStagingBufferSize = IsStaticLightBufferReallocated ? (StaticLightBufferAllocator.GetCapacity() - StaticLightBufferAllocator.GetTotalFreeSpace()) : (StaticLightsToUpdate.size() * SizeOfLightData);
	if (!InputStaticLights.empty() && !StaticLightStagingBufferSize)
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Unable to proceed light updating operation, unexpected transfer operation size!");
		throw std::runtime_error("Unable to proceed light updating operation, unexpected transfer operation size!");
	}

	std::vector<VkBufferCopy> AppendedStaticLightCopyRegions;
	std::vector<LightData> AppendedStaticLightDatas;
	
	if (IsStaticLightBufferReallocated)
	{
		for (auto& [LightPtr, LightEntry] : CurrentLightEntries.StaticLightLights)
		{
			VkBufferCopy CopyRegion{};
			CopyRegion.dstOffset = LightEntry.MemoryRegion.Offset;
			CopyRegion.size = SizeOfLightData;
			CopyRegion.srcOffset = AppendedStaticLightDatas.size() * SizeOfLightData;

			AppendedStaticLightCopyRegions.push_back(std::move(CopyRegion));
			AppendedStaticLightDatas.push_back(LightPtr->Data);
		}
	}
	else
	{
		for (auto& [LightPtr, AllocatedMemoryRegion] : StaticLightsToUpdate)
		{
			VkBufferCopy CopyRegion{};
			CopyRegion.dstOffset = AllocatedMemoryRegion->Offset;
			CopyRegion.size = SizeOfLightData;
			CopyRegion.srcOffset = AppendedStaticLightDatas.size() * SizeOfLightData;
			
			AppendedStaticLightCopyRegions.push_back(std::move(CopyRegion));
			AppendedStaticLightDatas.push_back(LightPtr->Data);
		}
	}

	std::shared_ptr<RENDERER::DataBlock> StaticLightDataBlock = std::make_shared<RENDERER::DataBlock>();
	StaticLightDataBlock->DataPtr = reinterpret_cast<uint8_t*>(AppendedStaticLightDatas.data());
	StaticLightDataBlock->SizeInBytes = AppendedStaticLightDatas.size() * SizeOfLightData;
	StaticLightDataBlock->Deleter = [LocalVector = std::move(AppendedStaticLightDatas)]() {};

	ResourceManagerPtr->RequestBufferCopyOperation(
		false,
		AppendedStaticLightCopyRegions,
		&StaticLightBuffer.Buffer,
		StaticLightDataBlock,
		3,
		RENDERER::COPY_OPERATION_FLAG_ATOMIC
	);

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
}
