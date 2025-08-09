#pragma once
#include <array>
#include <unordered_map>

#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/MemoryArenaAllocator.hpp"

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

	struct LightAppendInfo
	{
		std::vector<Light*> Lights;
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
		LightManager() = default;
		void Destroy(VkDevice& LogicalDevice);

		void AppendLights(LightAppendInfo &Info);
		void EraseLights(LightEraseInfo &Info);
	private:
		std::array<RENDERER_CORE::BufferAllocator, MAX_FRAMES_IN_FLIGHT> StaticLightSSBOs;
		std::array<RENDERER_CORE::PersistentBufferAllocator, MAX_FRAMES_IN_FLIGHT> DynamicLightSSBOs;

		RENDERER_CORE::PersistentBuffer StaticLightStagingBuffer{};

		std::array<LightEntryManager, MAX_FRAMES_IN_FLIGHT> LightEntries;
	};
}
