#pragma once
#include "VulkanPipeline.hpp"

#include "../../Common/StableVector.hpp"
#include "../../Common/Referenceable.hpp"

#include <unordered_map>

namespace RENDERER
{
	class RendererContext;
}

namespace RENDERER_CORE
{	
	struct GraphicsPipelineEntry : public COMMON::Referenceable<uint32_t>
	{
		RENDERER_CORE::GraphicsPipeline Pipeline;
		RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo;
	};

	struct ComputePipelineEntry : public COMMON::Referenceable<uint32_t>
	{
		RENDERER_CORE::ComputePipeline Pipeline;
		RENDERER_CORE::ComputePipelineCreateInfo PipelineCreateInfo;
	};

	class PipelineManager 
	{
		friend class RENDERER::RendererContext;
	public:
		PipelineManager(size_t ReserveCount);
		PipelineManager() = default;
		void Destroy(VkDevice LogicalDevice);
		void Create(size_t ReserveCount);

		//Appends a graphics pipeline and returns it's array index and a pointer to the entry.
		std::pair<GraphicsPipelineEntry*,size_t> AppendGraphicsPipeline(RENDERER_CORE::GraphicsPipelineCreateInfo &CreateInfo,VkDevice LogicalDevice);
		//Appends a compute pipeline and returns it's array index and a pointer to the entry.
		std::pair<ComputePipelineEntry*,size_t> AppendComputePipeline(RENDERER_CORE::ComputePipelineCreateInfo &CreateInfo,VkDevice LogicalDevice);

		GraphicsPipelineEntry* GetGraphicsPipeline(size_t Index);
		ComputePipelineEntry* GetComputePipeline(size_t Index);

		void EraseGraphicsPipelineByHash(size_t HashValue, VkDevice LogicalDevice);
		void EraseGraphicsPipelineByIndex(size_t Index, VkDevice LogicalDevice);
		void EraseComputePipelineByHash(size_t HashValue, VkDevice LogicalDevice);
		void EraseComputePipelineByIndex(size_t Index, VkDevice LogicalDevice);
	private:
		COMMON::StableVector<GraphicsPipelineEntry> GraphicPipelines;
		std::unordered_map<size_t,size_t> GraphicsPipelineHashTable;
		COMMON::StableVector<ComputePipelineEntry> ComputePipelines;
		std::unordered_map<size_t,size_t> ComputePipelinesHashTable;
	};
}