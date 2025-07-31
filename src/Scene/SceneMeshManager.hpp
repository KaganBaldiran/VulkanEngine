#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Common/CommonDefinitions.hpp"
#include "../Common/MemoryArenaAllocator.hpp"
#include "../Renderer/Core/VulkanDescriptorSet.hpp"

#include "Mesh.hpp"
#include "Material.hpp"

namespace VKAPP
{
	class RendererContext;
}

namespace VKSCENE
{
	//Forward Declarations;
	class ModelInstance;
	class TextureImportManager;

	template<uint32_t BufferSetCount>
	struct MeshDrawArenaBufferGroup
	{
		std::array<VKCORE::BufferAllocator, BufferSetCount> VertexBuffers;
		std::array<VKCORE::BufferAllocator, BufferSetCount> IndexBuffers;
		std::array<VKCORE::BufferAllocator, BufferSetCount> IndirectBuffers;
		std::array<VKCORE::PersistentBufferAllocator, BufferSetCount> ModelMatricesBuffers;

		std::array<VKCORE::BufferAllocator, BufferSetCount> DrawMetaDataBuffer;
		std::array<VKCORE::BufferAllocator, BufferSetCount> TexturesIndexBuffers;

		std::array<size_t, BufferSetCount> EnabledMeshCount;

		inline void Create()
		{
			EnabledMeshCount.fill(0);
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				VertexBuffers[i].Create();
				IndexBuffers[i].Create();
				IndirectBuffers[i].Create();
				ModelMatricesBuffers[i].Create();
				DrawMetaDataBuffer[i].Allocator.Create(0, VKCORE::MEMORY_SIZE_MEGABYTE);
				TexturesIndexBuffers[i].Allocator.Create(0, VKCORE::MEMORY_SIZE_MEGABYTE);
			}
		}

		inline void Destroy(VkDevice& LogicalDevice)
		{
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				VertexBuffers[i].Buffer.Destroy(LogicalDevice);
				IndexBuffers[i].Buffer.Destroy(LogicalDevice);
				IndirectBuffers[i].Buffer.Destroy(LogicalDevice);
				ModelMatricesBuffers[i].Buffer.Destroy(LogicalDevice);
				DrawMetaDataBuffer[i].Buffer.Destroy(LogicalDevice);
				TexturesIndexBuffers[i].Buffer.Destroy(LogicalDevice);
			}
		}
	};

	struct MeshMetaData
	{
		ModelInstance* ModelInstance;
		DrawInfo DrawInfo;
		size_t ModelIndex = 0;
		std::array<VKCORE::MemoryRegion,3> MemoryRegions;
	};

	struct MeshEntry
	{
		Mesh* MeshPtr;
		MeshMetaData MetaData;
	};

	struct ModelMetaData
	{
		uint32_t Index,ReferenceCount = 0;
	};

	struct ModelEntry
	{
		ModelMetaData MetaData;
		std::map<ModelInstance* , std::array<VKCORE::MemoryRegion, 2>, std::less<ModelInstance*>> Instances;
		std::vector<MeshEntry> MeshEntries;
		//std::unordered_map<Mesh*, MeshMetaData> MeshEntries;
	};

	struct ModelEntryManager
	{
		std::map<Model3D*, ModelEntry, std::less<Model3D*>> ModelEntries;
	};

	struct MeshAppendInfo
	{
		std::vector<ModelInstance*> ModelInstances;
		uint32_t FrameIndex;
		std::vector<VkDescriptorSet> TargetDescriptorSets;
	};

	struct MeshTextureUpdateInfo
	{
		TextureImportManager *TextureImportManagerPtr;
		uint32_t FrameIndex;
		std::array<VkDescriptorSet,MAX_FRAMES_IN_FLIGHT> TargetDescriptorSets;
	};

	struct MeshEraseInfo
	{
		std::vector<ModelInstance*> ModelInstances;
		uint32_t FrameIndex;
		std::vector<VkDescriptorSet> TargetDescriptorSets;
	};

	struct ExtendedIndirectCommand
	{
		int IndexCount;
		int InstanceCount;
		int FirstIndex;
		int VertexOffset;
		int FirstInstance;
		int MeshIndex;
	};

	struct DrawMetadata {
		int MeshID;       
		int ModelMatrixIndex;
	};
	
	class MeshManager
	{
		friend class Scene;
	public:
		MeshManager(VKAPP::RendererContext& RendererContext);
		MeshManager() = default;
		void Create(VKAPP::RendererContext& RendererContext);
		void Destroy(VkDevice& LogicalDevice);

		MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> PerformanceModeBuffers;
		//MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> BalancedModeBuffers;
		//MeshDrawArenaBufferGroup<1> MemorySavingModeBuffers;

		uint32_t CurrentBalancedBuffer = 0;
		std::vector<VKCORE::Buffer> SceneMeshIndexBuffers;

		VKAPP::RendererContext* RendererContext = nullptr;
		std::array<ModelEntryManager,MAX_FRAMES_IN_FLIGHT> ModelEntries;
		
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		void AppendModels(MeshAppendInfo Info);
		void EraseModels(MeshEraseInfo Info);
		void UpdateTextureDescriptors(MeshTextureUpdateInfo Info);
	private:
		void WriteTexture(
			uint32_t TextureTypeIndex,
			VKSCENE::Mesh& Mesh,
			VKSCENE::TextureImportManager& TextureImportManager,
			std::vector<VKCORE::DescriptorSetWriteImage>& ImageWrites,
			std::vector<int>& TextureIndexes,
			int& CurrentImageIndex,
			VkDescriptorSet DestinationDescriptorSet
		);

		Scene* Owner = nullptr;
	};
}
