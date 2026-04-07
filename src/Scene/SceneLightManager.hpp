#pragma once
#include <array>
#include <unordered_map>

#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/MemoryArenaAllocator.hpp"

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	class Light;

	struct LightEntry
	{
		RENDERER_CORE::MemoryRegion MemoryRegion{};
	};

	struct LightEntryManager
	{
		std::unordered_map<Light*, LightEntry> DynamicLights;
		std::unordered_map<Light*, LightEntry> StaticLightLights;
	};

	struct LightAppendOrUpdateInfo
	{
		std::vector<Light*> StaticLights;
		std::vector<Light*> DynamicLights;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> TargetDescriptorSets;
		uint32_t FrameIndex;
	};

	struct LightEraseInfo
	{
		std::vector<Light*> Lights;
		uint32_t FrameIndex;
	};

	class LightManager
	{
	public:
		LightManager(RENDERER::RendererContext& RendererContext);
		LightManager() = default;
		void Create(RENDERER::RendererContext &RendererContext);
		void Destroy(VkDevice& LogicalDevice);

		void AppendOrUpdateLights(LightAppendOrUpdateInfo &Info);
		void EraseLights(LightEraseInfo &Info);
		std::array<LightEntryManager, MAX_FRAMES_IN_FLIGHT> LightEntries;
		std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT> StaticLightSSBOs;
		std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT> DynamicLightSSBOs;
	private:
		RENDERER::RendererContext* RendererContext;
	};
}
