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
#include "../Common/VectorMap.hpp"
#include "../Renderer/Core/VulkanDescriptorSet.hpp"

#include "Mesh.hpp"
#include "Material.hpp"
#include "PersistentSceneStagingBuffer.hpp"

namespace RENDERER
{
	class RendererContext;
}

namespace SCENE
{
	//Forward Declarations;
	class ModelInstance;
	class TextureManager;
	class MeshManager;
	enum SceneDynamicUploadMode;
	struct SceneOptions;

	namespace INTERNAL
	{
		struct TextureIndexData
		{
			int AlbedoTextureIndex;
			int RoughnessTextureIndex;
			int NormalMapTextureIndex;
			int MetallicTextureIndex;
			int OpacityTextureIndex;
		};

		struct MaterialParameterData
		{
			glm::vec3 Albedo;
			float Metallic;
			float Roughness;
		};

		struct MaterialSamplingData
		{
			glm::vec3 Albedo;
			float Metallic;
			float Roughness;
		};

		struct MaterialData
		{
			TextureIndexData IndexData;
		};

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
					MeshVisibilityCountBuffers[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_KILOBYTE);
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
			//MeshFrustumCullBuffers<BufferSetCount> CullBuffers;
			std::array<RENDERER_CORE::PersistentBufferAllocator, BufferSetCount> ModelMatricesBuffers;
			std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> DrawMetaDataBuffer;
			std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> TexturesIndexBuffers;

			std::array<size_t, BufferSetCount> EnabledMeshCount;
			std::array<bool, BufferSetCount> TexturesIndexBuffersReallocated;

			inline void Create(size_t BufferAllocationStep)
			{
				EnabledMeshCount.fill(0);
				for (size_t i = 0; i < BufferSetCount; i++)
				{
					IndirectBuffers[i].Allocator.Create(0, BufferAllocationStep);
					ModelMatricesBuffers[i].Allocator.Create(0, BufferAllocationStep);
					DrawMetaDataBuffer[i].Allocator.Create(0, BufferAllocationStep);
					TexturesIndexBuffers[i].Allocator.Create(0, BufferAllocationStep);
					//CullBuffers.MeshVisibilityCountBuffers[i].Allocator.Create(0, RENDERER_CORE::MEMORY_SIZE_KILOBYTE);
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
					//CullBuffers.MeshVisibilityCountBuffers[i].Buffer.Destroy(LogicalDevice);
					//CullBuffers.CulledIndirectBuffers[i].Destroy(LogicalDevice);
					//CullBuffers.CulledDrawMetaDataBuffer[i].Destroy(LogicalDevice);
				}
			}
		};

	
		struct InstanceMeshLink
		{
			size_t ResourceID;
			RENDERER_CORE::MemoryRegion DrawDataMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingDrawDataMemoryRegion;
		};

		struct MeshEntry
		{
			uint32_t Index, ReferenceCount = 0;
			bool IsChanged = false;
			size_t ResourceID, FirstInstance = std::numeric_limits<size_t>::max();
			RENDERER_CORE::MemoryRegion IndirectBufferMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingIndirectBufferMemoryRegion;
			DrawInfo Info;
			BoundingBoxAABB BoundingBox;
			std::vector<InstanceMeshLink> InstanceLinks;
		};

		struct MaterialMetaData
		{
			MaterialData Material;
			std::array<uint32_t, static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE)> TextureIndexes;
			RENDERER_CORE::MemoryRegion TextureIndexMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingTextureIndexMemoryRegion;
		};

		//Data related to instance entry
		struct InstanceEntry
		{
			std::unordered_map<size_t, MaterialMetaData> Materials;
			RENDERER_CORE::MemoryRegion TransformationMatrixMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingTransformationMatrixMemoryRegion;
			glm::mat4 ModelMatrix;
		};

		//General layer of the entries. Unique for each frame in flight
		struct EntryManager
		{
			std::unordered_map<size_t, InstanceEntry> InstanceEntries;
			COMMON::VectorMap<size_t, MeshEntry> MeshEntries;

			std::vector<ModelInstance*> MaterialUpdateList;
			//A flag to matrix buffer updating function to let it know when to recopy everything inside.
			bool TransformationMatrixReallocated = false;
		};

		struct DrawMetadata {
			int MeshID;
			int ModelMatrixIndex;
		};
	}

	struct MeshEraseInfo
	{
		std::vector<ModelInstance*> ModelInstances;
		uint32_t FrameIndex;
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> TargetDescriptorSets;
	};

	struct ExtendedIndirectCommand
	{
		int IndexCount;
		int InstanceCount;
		int FirstIndex;
		int VertexOffset;
		int FirstInstance;
		//BoundingBoxAABB BoundingBox;
	};

	
	// Centralized class to manage buffers needed for indirect rendering.
	// It packs the meshes tightly into centralized classes to reduce state changes.
	// It's meant for internal usage in the scene class but can easily be adapted for special use cases.
	class SceneMeshManager
	{
		friend class Scene;
	public:
		SceneMeshManager(MeshManager& MeshManager,RENDERER::RendererContext& RendererContext,size_t BufferAllocationStep);
		SceneMeshManager() = default;
		void Create(MeshManager& MeshManager,RENDERER::RendererContext& RendererContext, size_t BufferAllocationStep);
		void Destroy(VkDevice& LogicalDevice);

		INTERNAL::MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> SceneBuffers;

		uint32_t CurrentBalancedBuffer = 0;
		std::array<INTERNAL::EntryManager,MAX_FRAMES_IN_FLIGHT> Entries;
		
		void UpdateMeshTransformationsHostVisible(std::vector<ModelInstance*> &UpdateList,uint32_t CurrentFrame);
		void UpdateMeshTransformationsDeviceLocal(
			std::vector<ModelInstance*>& UpdateList,
			uint32_t CurrentFrame,
			std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(SCENE::BUFFER_COPY_SLOT_SIZE)>& CurrentCopyInfos,
			SCENE::PersistentStagingBuffer& StagingBuffer
		);

		void ResetModels(uint32_t FrameIndex);
		void AppendModels(
			std::vector<ModelInstance*> &ModelInstances,
			const uint32_t &FrameIndex,
			std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> &TargetDescriptorSets,
			PersistentStagingBuffer &StagingBuffer,
			SceneOptions SceneOptions,
			std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
		);
		void EraseModels(MeshEraseInfo Info);
		void UpdateMaterials(
			std::vector<SCENE::ModelInstance*>& SceneMaterialUpdateList,
			TextureManager* TextureImportManagerPtr,
			uint32_t FrameIndex,
			VkDescriptorSet& TargetDescriptorSet,
			PersistentStagingBuffer& StagingBuffer,
			std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
		);
	private:
		MeshManager* MeshManagerPtr = nullptr;
		RENDERER::RendererContext* RendererContext = nullptr;
	};
}
