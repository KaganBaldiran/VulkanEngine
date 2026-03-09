#include "PipelineManager.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"

RENDERER_CORE::PipelineManager::PipelineManager(size_t ReserveCount)
{
	Create(ReserveCount);
}

void RENDERER_CORE::PipelineManager::Create(size_t ReserveCount)
{
	this->GraphicPipelines.reserve(ReserveCount);
	this->ComputePipelines.reserve(ReserveCount);
	this->ComputePipelinesHashTable.reserve(ReserveCount);
	this->GraphicsPipelineHashTable.reserve(ReserveCount);
}

std::pair<RENDERER_CORE::GraphicsPipelineEntry*, size_t> RENDERER_CORE::PipelineManager::AppendGraphicsPipeline(RENDERER_CORE::GraphicsPipelineCreateInfo& CreateInfo, VkDevice LogicalDevice)
{
	const size_t Hash = CreateInfo.Hash();

	auto PipelineIterator = GraphicsPipelineHashTable.find(Hash);
	if (PipelineIterator != GraphicsPipelineHashTable.end())
	{
		auto& PipelineEntry = GraphicPipelines[PipelineIterator->second];
		PipelineEntry.IncreaseReference();
		return { &PipelineEntry,PipelineIterator->second};
	}	
	GraphicsPipeline NewPipeline(CreateInfo,LogicalDevice);
	NewPipeline.Hash = Hash;

	GraphicsPipelineEntry NewEntry{};
	NewEntry.Pipeline = std::move(NewPipeline);
	NewEntry.IncreaseReference();
	NewEntry.PipelineCreateInfo = CreateInfo;

	size_t Index = GraphicPipelines.push_back(std::move(NewEntry));
	GraphicsPipelineHashTable.insert({ Hash,Index });
	return { &GraphicPipelines[Index],Index};
}

std::pair<RENDERER_CORE::ComputePipelineEntry*, size_t> RENDERER_CORE::PipelineManager::AppendComputePipeline(RENDERER_CORE::ComputePipelineCreateInfo& CreateInfo, VkDevice LogicalDevice)
{
	const size_t Hash = CreateInfo.Hash();
	auto PipelineIterator = ComputePipelinesHashTable.find(Hash);
	if (PipelineIterator != ComputePipelinesHashTable.end())
	{
		auto& PipelineEntry = ComputePipelines[PipelineIterator->second];
		PipelineEntry.IncreaseReference();
		return { &PipelineEntry,PipelineIterator->second };
	}
	ComputePipeline NewPipeline(CreateInfo, LogicalDevice);
	NewPipeline.Hash = Hash;

	ComputePipelineEntry NewEntry{};
	NewEntry.Pipeline = std::move(NewPipeline);
	NewEntry.IncreaseReference();
	NewEntry.PipelineCreateInfo = CreateInfo;

	size_t Index = ComputePipelines.push_back(std::move(NewEntry));
	ComputePipelinesHashTable.insert({ Hash,Index });
	return { &ComputePipelines[Index],Index };
}

RENDERER_CORE::GraphicsPipelineEntry* RENDERER_CORE::PipelineManager::GetGraphicsPipeline(size_t Index)
{
	if (!GraphicPipelines.valid(Index))
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING, 
			"Attempting to retrieve a graphics pipeline with an invalid index (" + std::to_string(Index) + ").");
		return nullptr;
	}
	return &GraphicPipelines[Index];
}

RENDERER_CORE::ComputePipelineEntry* RENDERER_CORE::PipelineManager::GetComputePipeline(size_t Index)
{
	if (!ComputePipelines.valid(Index))
	{
		LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING,
			"Attempting to retrieve a compute pipeline with an invalid index (" + std::to_string(Index) + ").");
		return nullptr;
	}
	return &ComputePipelines[Index];
}

void RENDERER_CORE::PipelineManager::EraseGraphicsPipelineByHash(size_t HashValue,VkDevice LogicalDevice)
{
	auto PipelineIterator = GraphicsPipelineHashTable.find(HashValue);
	if (PipelineIterator == GraphicsPipelineHashTable.end())
	{
		return;
	}
	auto& PipelineEntry = GraphicPipelines[PipelineIterator->second];
	if (!PipelineEntry.DecreaseReference())
	{
		GraphicsPipelineHashTable.erase(PipelineIterator);
		GraphicPipelines[PipelineIterator->second].Pipeline.Destroy(LogicalDevice);
		GraphicPipelines.erase(PipelineIterator->second);
	}
}

void RENDERER_CORE::PipelineManager::EraseGraphicsPipelineByIndex(size_t Index, VkDevice LogicalDevice)
{
	auto PipelineIterator = GetGraphicsPipeline(Index);
	if (!PipelineIterator)
	{
		return;
	}
	if (!PipelineIterator->DecreaseReference())
	{
		GraphicsPipelineHashTable.erase(PipelineIterator->Pipeline.GetHash());
		PipelineIterator->Pipeline.Destroy(LogicalDevice);
		GraphicPipelines.erase(Index);
	}
}

void RENDERER_CORE::PipelineManager::EraseComputePipelineByHash(size_t HashValue,VkDevice LogicalDevice)
{
	auto PipelineIterator = ComputePipelinesHashTable.find(HashValue);
	if (PipelineIterator == ComputePipelinesHashTable.end())
	{
		return;
	}
	auto& PipelineEntry = ComputePipelines[PipelineIterator->second];
	if (!PipelineEntry.DecreaseReference())
	{
		ComputePipelinesHashTable.erase(PipelineIterator);
		ComputePipelines[PipelineIterator->second].Pipeline.Destroy(LogicalDevice);
		ComputePipelines.erase(PipelineIterator->second);
	}
}

void RENDERER_CORE::PipelineManager::EraseComputePipelineByIndex(size_t Index, VkDevice LogicalDevice)
{
	auto PipelineIterator = GetComputePipeline(Index);
	if (!PipelineIterator)
	{
		return;
	}
	if (!PipelineIterator->DecreaseReference())
	{
		ComputePipelinesHashTable.erase(PipelineIterator->Pipeline.GetHash());
		PipelineIterator->Pipeline.Destroy(LogicalDevice);
		ComputePipelines.erase(Index);
	}
}

void RENDERER_CORE::PipelineManager::Destroy(VkDevice LogicalDevice)
{
	for (auto& [Hash,PipelineIndex] : GraphicsPipelineHashTable)
	{
		GraphicPipelines[PipelineIndex].Pipeline.Destroy(LogicalDevice);
	}
	GraphicsPipelineHashTable.clear();
	GraphicPipelines.clear();

	for (auto& [Hash, PipelineIndex] : ComputePipelinesHashTable)
	{
		ComputePipelines[PipelineIndex].Pipeline.Destroy(LogicalDevice);
	}
	ComputePipelinesHashTable.clear();
	ComputePipelines.clear();
}


