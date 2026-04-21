#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>
#include <map>
#include <string>

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/MemoryArenaAllocator.hpp"

#include "../Renderer/Core/VulkanImage.hpp"
#include "../Renderer/Core/VulkanDescriptor.hpp"
#include "../Renderer/Core/VulkanPipeline.hpp"
#include "Renderer/RendererContext.hpp"

namespace SCENE
{
	class SceneMeshManager;
}

namespace RENDERER
{
	class RendererContext;
	class ResourceManager;
	class Renderer;
	class DeferredRenderPipeline;

	struct TextureImportInfo
	{
		std::string FileName;
		uint64_t DestinationTextureID;
	};

	struct TextureDataEntry
	{
		RENDERER_CORE::ImageData Data;
		std::array<size_t,MAX_FRAMES_IN_FLIGHT> DescriptorSlots;
	};

	class TextureManager : COMMON::Destructible
	{
		friend class SCENE::SceneMeshManager;
		friend class Renderer;
		friend class DeferredRenderPipeline;
	private:
		std::mutex Mutex;
		double StartingTime;
		RendererContext* RendererContextPtr = nullptr;
		ResourceManager* ResourceManagerPtr = nullptr;
		void UpdateDescriptors(uint32_t FrameIndex);

		std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> DescriptorWriteQueue;

		struct TextureImportLoad
		{
			std::shared_ptr<RENDERER_CORE::RawImageData> RawData;
			uint64_t DestinationTextureID;
		};

		struct TextureMemoryAllocation
		{
			RENDERER_CORE::MemoryRegion Region;
			VkDeviceMemory Memory = VK_NULL_HANDLE;
		};

		struct TextureMemoryPage
		{
			VkDeviceMemory Memory = VK_NULL_HANDLE;
			RENDERER_CORE::VirtualArenaAllocator Allocator;
		};
		std::unordered_map<uint32_t, std::vector<TextureMemoryPage>> TexturePages;
		TextureMemoryAllocation AllocateFromTexturePages(
			VkMemoryRequirements MemoryRequirements,
			VkMemoryPropertyFlags Properties,
			VkPhysicalDevice PhysicalDevice,
			VkDevice LogicalDevice
		);
	public:
		TextureManager(RendererContext& RendererContext,RENDERER::ResourceManager& ResourceManagerPtr);
		TextureManager() = default;
		void Create(RendererContext& RendererContext,RENDERER::ResourceManager& ResourceManagerPtr);
		void Destroy() override;

		void AppendImportTask(TextureImportInfo ImportInfo);
		void SubmitImport();
		void WaitImportsIdle();

		std::vector<std::pair<TextureImportInfo,std::future<TextureImportLoad>>> Futures;
		std::queue<TextureImportInfo> ImportQueue;

		std::unordered_map<std::string,uint64_t> ImportRegistries;
		std::unordered_map<uint64_t,std::shared_ptr<RENDERER_CORE::RawImageData>> RawImageDatas;
		std::unordered_map<uint64_t,TextureDataEntry> TextureDatas;
	};
}