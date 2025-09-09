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
#include "SceneResource.hpp"

namespace RENDERER
{
	class RendererContext;
	class Renderer;
	class DeferredRenderPipeline;
}

namespace SCENE
{
	class Texture : public Resource
	{
	public:
	private:
	};

	struct TextureImportInfo
	{
		std::string FileName;
		uint64_t DestinationTextureID;
	};

	struct TextureDataEntry
	{
		RENDERER_CORE::TextureData Data;
		std::array<size_t,MAX_FRAMES_IN_FLIGHT> DescriptorSlots;
	};

	class TextureImportManager : COMMON::Destructible
	{
		friend class SceneMeshManager;
		friend class RENDERER::Renderer;
		friend class RENDERER::DeferredRenderPipeline;
	public:
		TextureImportManager(RENDERER::RendererContext& RendererContext);
		TextureImportManager() = default;
		void Create(RENDERER::RendererContext& RendererContext);
		void Destroy() override;

		void AppendImportTask(TextureImportInfo ImportInfo);
		void SubmitImport();

		std::vector<std::pair<TextureImportInfo,std::future<bool>>> Futures;
		std::queue<TextureImportInfo> ImportQueue;

		std::unordered_map<std::string,uint64_t> ImportRegistries;
		std::unordered_map<uint64_t,RENDERER_CORE::RawImageData> RawImageDatas;
		std::unordered_map<uint64_t,TextureDataEntry> TextureDatas;
	private:
		double StartingTime;
		RENDERER::RendererContext* RendererContext = nullptr;
		bool CreateMeshTextureDescriptors(uint32_t DescriptorCount, uint32_t FrameIndex);
		void DestroyMeshTextureDescriptors();
		void UpdateDescriptors(uint32_t FrameIndex);

		std::array<std::vector<uint32_t>, MAX_FRAMES_IN_FLIGHT> DescriptorWriteQueue;
		// 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Texture array
		std::array<RENDERER_CORE::Descriptor<1>, MAX_FRAMES_IN_FLIGHT> TexturesDescriptors;
		std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> TextureDescriptorUpperBounds;
		std::array<RENDERER_CORE::VirtualArenaAllocator, MAX_FRAMES_IN_FLIGHT> TextureDescriptorIndexAllocators;

		std::array<RENDERER_CORE::GraphicsPipeline, 3>* TextureDescriptorsPipelines = nullptr;
		RENDERER_CORE::GraphicsPipeline* CurrentShadingPassPipeline = nullptr;
	};
}