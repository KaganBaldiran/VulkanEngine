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
#include "../Renderer/Core/VulkanDescriptorSet.hpp"
#include "../Renderer/ResourceManager.hpp"

#include "../Common/CommonDefinitions.hpp"
#include "../Common/MemoryArenaAllocator.hpp"
#include "../Common/VectorMap.hpp"

#include "Mesh.hpp"
#include "Material.hpp"
#include "PersistentSceneStagingBuffer.hpp"

namespace RENDERER
{
	class RendererContext;
	//class ResourceManager;
}

namespace SCENE
{
	//Forward Declarations;
	class ModelInstance;
	class TextureManager;
	enum SceneDynamicUploadMode;
	struct SceneOptions;

	namespace INTERNAL
	{
		struct ModelTransformMatrices
		{
			glm::mat4 ModelMatrix;
			glm::mat4 NormalMatrix;
		};

		struct MaterialTextureIndexData
		{
			std::array<uint32_t, static_cast<uint32_t>(MATERIAL_TEXTURE_TYPE_META_DATA_SIZE)> TextureIndexes;
			bool operator==(const MaterialTextureIndexData& Data0) const { return this->TextureIndexes == Data0.TextureIndexes; }
		};

		struct MaterialParameterData
		{
			float Roughness;
			float Metallic;
			float Padding;
			glm::vec4 Albedo;

			bool operator==(const MaterialParameterData& Data0) const
			{
				return this->Roughness == Data0.Roughness &&
					   this->Metallic == Data0.Metallic && 
					   this->Albedo == Data0.Albedo;
			}
		};

		struct MaterialSamplingData
		{
			glm::vec2 TextureSamplePosition;
			glm::vec2 TextureSampleSize;

			bool operator==(const MaterialSamplingData& Data0) const 
			{
				return this->TextureSampleSize == Data0.TextureSampleSize && this->TextureSamplePosition == Data0.TextureSamplePosition;
			}
		};

		struct alignas(16) MaterialData
		{
			MaterialTextureIndexData IndexData;
			MaterialParameterData Parameters;
			MaterialSamplingData SamplingData;

			bool operator==(const MaterialData& Data0) const
			{
				return this->IndexData == Data0.IndexData &&
					   this->Parameters == Data0.Parameters &&
					   this->SamplingData == Data0.SamplingData;
			}
		};

		template<uint32_t BufferSetCount>
		struct MeshFrustumCullBuffers
		{
			std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> VisibilityIndexBuffers;

			inline void Create(size_t BufferAllocationStep)
			{
				for (size_t i = 0; i < BufferSetCount; i++)
				{
					VisibilityIndexBuffers[i].Allocator.Create(0, BufferAllocationStep);
				}
			}
			inline void Destroy(VkDevice& LogicalDevice)
			{
				for (size_t i = 0; i < BufferSetCount; i++)
				{
					RENDERER_CORE::DestroyBuffer(LogicalDevice, VisibilityIndexBuffers[i].Buffer);
				}
			}
		};

		template<uint32_t BufferSetCount>
		struct MeshDrawArenaBufferGroup
		{
			std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> IndirectBuffers;
			MeshFrustumCullBuffers<BufferSetCount> CullBuffers;
			std::array<RENDERER_CORE::BufferAllocator, BufferSetCount> ModelMatricesBuffers;
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
				CullBuffers.Create(BufferAllocationStep);
			}

			inline void Destroy(VkDevice& LogicalDevice)
			{
				for (size_t i = 0; i < BufferSetCount; i++)
				{
					RENDERER_CORE::DestroyBuffer(LogicalDevice, IndirectBuffers[i].Buffer);
					RENDERER_CORE::DestroyBuffer(LogicalDevice, ModelMatricesBuffers[i].Buffer);
					RENDERER_CORE::DestroyBuffer(LogicalDevice, DrawMetaDataBuffer[i].Buffer);
					RENDERER_CORE::DestroyBuffer(LogicalDevice, TexturesIndexBuffers[i].Buffer);
					//CullBuffers.MeshVisibilityCountBuffers[i].Buffer.Destroy(LogicalDevice);
					//CullBuffers.CulledIndirectBuffers[i].Destroy(LogicalDevice);
					//CullBuffers.CulledDrawMetaDataBuffer[i].Destroy(LogicalDevice);
				}
				CullBuffers.Destroy(LogicalDevice);
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
			uint32_t PageIndex;
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
			RENDERER_CORE::MemoryRegion TextureIndexMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingTextureIndexMemoryRegion;
		};

		//Data related to instance entry
		struct InstanceEntry
		{
			std::unordered_map<size_t, MaterialMetaData> Materials;
			RENDERER_CORE::MemoryRegion TransformationMatrixMemoryRegion;
			RENDERER_CORE::MemoryRegion StagingTransformationMatrixMemoryRegion;
			ModelTransformMatrices TransformMatrices;
			//glm::mat4 ModelMatrix;
		};

		//General layer of the entries. Unique for each frame in flight
		struct EntryManager
		{
			COMMON::VectorMap<size_t, InstanceEntry> InstanceEntries;
			COMMON::VectorMap<size_t, MeshEntry> MeshEntries;
			//A flag to matrix buffer updating function to let it know when to recopy everything inside.
			bool TransformationMatrixReallocated = false;
		};

		struct DrawMetadata {
			int MaterialID;
			int MeshID;
			int ModelMatrixIndex;
		};

		struct PageMeshCountEntry
		{
			size_t MeshCount = 0;
			size_t InstanceCount = 0;
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
		uint32_t IndexCount;
		uint32_t InstanceCount;
		uint32_t FirstIndex;
		int32_t VertexOffset;
		uint32_t FirstInstance;
		BoundingBoxAABB BoundingBox;
	};

	
	// Centralized class to manage buffers needed for indirect rendering.
	// It packs the meshes tightly into centralized classes to reduce state changes.
	// It's meant for internal usage in the scene class but can easily be adapted for special use cases.
	class SceneMeshManager
	{
		friend class Scene;
	public:
		SceneMeshManager(RENDERER::ResourceManager& ResourceManager,RENDERER::RendererContext& RendererContext,size_t BufferAllocationStep);
		SceneMeshManager() = default;
		void Create(RENDERER::ResourceManager& ResourceManager,RENDERER::RendererContext& RendererContext, size_t BufferAllocationStep);
		void Destroy(VkDevice& LogicalDevice);

		INTERNAL::MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> Buffers;

		uint32_t CurrentBalancedBuffer = 0;
		std::array<INTERNAL::EntryManager,MAX_FRAMES_IN_FLIGHT> Entries;
		//Array that holds mesh count related data per geometry buffer page.
		std::array<std::vector<SCENE::INTERNAL::PageMeshCountEntry>,MAX_FRAMES_IN_FLIGHT> PageMeshCounts;
		
		void UpdateMeshTransformationsHostVisible(std::vector<ModelInstance*> &UpdateList,uint32_t CurrentFrame);
		void UpdateMeshTransformationsDeviceLocal(
			std::vector<ModelInstance*>& UpdateList,
			uint32_t CurrentFrame
			//std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)>& CopyOperations,
			//SCENE::PersistentStagingBuffer& StagingBuffer
		);

		void ResetModels(uint32_t FrameIndex);
		void AppendModels(
			std::vector<ModelInstance*> &ModelInstances,
			std::vector<ModelInstance*>& MaterialUpdateList,
			const uint32_t &FrameIndex,
			std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> &TargetDescriptorSets,
			SceneOptions SceneOptions
			//std::array<RENDERER::CopyOperationEntry*, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
		);
		void EraseModels(MeshEraseInfo Info);
		void UpdateMaterials(
			std::vector<SCENE::ModelInstance*>& SceneMaterialUpdateList,
			uint32_t FrameIndex,
			VkDescriptorSet& TargetDescriptorSet
			//PersistentStagingBuffer& StagingBuffer,
			//std::array<RENDERER::CopyOperationEntry*, static_cast<size_t>(BUFFER_COPY_SLOT_SIZE)>& CopyInfos
		);
	private:
		//std::array<std::vector<ExtendedIndirectCommand>, MAX_FRAMES_IN_FLIGHT> AppendedIndirectCommands;

		RENDERER::ResourceManager* ResourceManagerPtr = nullptr;
		RENDERER::RendererContext* RendererContext = nullptr;
	};
}
