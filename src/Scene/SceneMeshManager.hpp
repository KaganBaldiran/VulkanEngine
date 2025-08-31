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

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	//Forward Declarations;
	class ModelInstance;
	class TextureImportManager;
	class MeshManager;

	template<uint32_t BufferSetCount>
	struct MeshFrustumCullBuffers
	{
		std::array<RENDERER_CORE::Buffer, BufferSetCount> CulledIndirectBuffers;
		std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> MeshVisibilityCountBuffers;
		std::array<RENDERER_CORE::Buffer, BufferSetCount> CulledDrawMetaDataBuffer;

		inline void Create()
		{
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				MeshVisibilityCountBuffers[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_MEGABYTE);
			}
		}

		inline void Destroy(VkDevice& LogicalDevice)
		{
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				MeshVisibilityCountBuffers[i].Buffer.Destroy(LogicalDevice);
				CulledIndirectBuffers[i].Buffer.Destroy(LogicalDevice);
				CulledDrawMetaDataBuffer[i].Buffer.Destroy(LogicalDevice);
			}
		}
	};

	template<uint32_t BufferSetCount>
	struct MeshDrawArenaBufferGroup
	{
		std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> IndirectBuffers;
		MeshFrustumCullBuffers<BufferSetCount> CullBuffers;
		std::array<RENDERER_CORE::PersistentBufferAllocator, BufferSetCount> ModelMatricesBuffers;
		std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> DrawMetaDataBuffer;
		std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> TexturesIndexBuffers;

		std::array<size_t, BufferSetCount> EnabledMeshCount;
		std::array<bool, BufferSetCount> TexturesIndexBuffersReallocated;

		inline void Create()
		{
			EnabledMeshCount.fill(0);
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				IndirectBuffers[i].Create();
				ModelMatricesBuffers[i].Create();
				DrawMetaDataBuffer[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_MEGABYTE);
				TexturesIndexBuffers[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_MEGABYTE);
				CullBuffers.MeshVisibilityCountBuffers[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_MEGABYTE);
			}
		}

		inline void Destroy(VkDevice& LogicalDevice)
		{
			for (size_t i = 0; i < BufferSetCount; i++)
			{
				IndirectBuffers[i].Buffer.Destroy(LogicalDevice);
				ModelMatricesBuffers[i].Buffer.Destroy(LogicalDevice);
				DrawMetaDataBuffer[i].Buffer.Destroy(LogicalDevice);
				TexturesIndexBuffers[i].Buffer.Destroy(LogicalDevice);
				CullBuffers.MeshVisibilityCountBuffers[i].Buffer.Destroy(LogicalDevice);
				CullBuffers.CulledIndirectBuffers[i].Destroy(LogicalDevice);
				CullBuffers.CulledDrawMetaDataBuffer[i].Destroy(LogicalDevice);
			}
		}
	};

	struct InstanceMeshLink
	{
		size_t ResourceID;
		RENDERER_CORE::MemoryRegion DrawDataMemoryRegion;
	};

	struct MeshEntry
	{
		uint32_t Index, ReferenceCount = 0;
		bool IsChanged = false;
		size_t ResourceID;
		RENDERER_CORE::MemoryRegion IndirectBufferMemoryRegion;
		DrawInfo Info;
		BoundingBoxAABB BoundingBox;
		std::vector<InstanceMeshLink> InstanceLinks;
	};

	struct MaterialMetaData
	{
		Material Material;
		std::array<uint32_t, static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE)> TextureIndexes;
		RENDERER_CORE::MemoryRegion TextureIndexMemoryRegion;
	};

	struct InstanceEntry
	{
		std::unordered_map<size_t,MaterialMetaData> Materials;
		RENDERER_CORE::MemoryRegion TransformationMatrixMemoryRegion;
	};

	struct EntryManager
	{
		std::map<size_t, InstanceEntry> InstanceEntries;
		std::map<size_t, MeshEntry> MeshEntries;

		std::vector<ModelInstance*> MaterialUpdateList;
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
		std::vector<VkDescriptorSet> TargetDescriptorSets;
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
		BoundingBoxAABB BoundingBox;
	};

	struct DrawMetadata {
		int MeshID;       
		int ModelMatrixIndex;
	};
	
	/// <summary>
	/// Centralized class to manage buffers needed for indirect rendering.
	/// It packs the meshes tightly into centralized classes to reduce state changes.
	/// It's meant for internal usage in the scene class but can easily be adapted for special use cases.
	/// </summary>
	class SceneMeshManager
	{
		friend class Scene;
	public:
		SceneMeshManager(MeshManager& MeshManager,RENDERER::RendererContext& RendererContext);
		SceneMeshManager() = default;
		void Create(MeshManager& MeshManager,RENDERER::RendererContext& RendererContext);
		void Destroy(VkDevice& LogicalDevice);

		MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> PerformanceModeBuffers;
		//MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> BalancedModeBuffers;
		//MeshDrawArenaBufferGroup<1> MemorySavingModeBuffers;

		uint32_t CurrentBalancedBuffer = 0;
		std::vector<RENDERER_CORE::Buffer> SceneMeshIndexBuffers;

		std::array<EntryManager,MAX_FRAMES_IN_FLIGHT> ModelEntries;
		
		void UpdateMeshTransformations(std::vector<ModelInstance*> &UpdateList,uint32_t CurrentFrame);

		void AppendModels(MeshAppendInfo Info);
		void EraseModels(MeshEraseInfo Info);
		void UpdateTextureDescriptors(MeshTextureUpdateInfo Info);
	private:
		void WriteTexture(
			uint32_t TextureTypeIndex,
			SCENE::Material& Material,
			SCENE::TextureImportManager& TextureImportManager,
			std::vector<RENDERER_CORE::DescriptorSetWriteImage>& ImageWrites,
			std::vector<int>& TextureIndexes,
			int& CurrentImageIndex,
			VkDescriptorSet DestinationDescriptorSet
		);

		MeshManager* MeshManagerPtr = nullptr;
		RENDERER::RendererContext* RendererContext = nullptr;
	};
}
