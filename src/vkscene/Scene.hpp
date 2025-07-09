#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"

#include "../vkphysics/DebugDrawer.hpp"
#include "../vkcore/VulkanDescriptorPool.hpp"
#include "../vkcore/VulkanDescriptorSet.hpp"
#include "../vkcore/VulkanDescriptorSetLayout.hpp"

#include "Light.hpp"
#include "Entity.hpp"

namespace VKCORE
{
	//Forward Declarations
	class Window;
	struct Buffer;
	class GraphicsPipeline;
}

namespace VKAPP
{
	class Renderer;
	class RendererContext;
}

namespace VKSCENE
{
	//Forward Declarations
	class Camera3D;
	class Cubemap;
	class SceneResource;
	class TextureImportManager;

	enum ScenePendingUpdateType
	{
		SCENE_PENDING_UPDATE_TYPE_NONE = 0,
		SCENE_PENDING_UPDATE_TYPE_STATIC_LIGHT_BUFFERS = 1,
		SCENE_PENDING_UPDATE_TYPE_DYNAMIC_LIGHT_BUFFERS = 2,
		SCENE_PENDING_UPDATE_TYPE_TEXTURE_DESCRIPTORS = 3
	};
	using SceneResourceUpdateCallback = bool;

	enum MeshUpdateModeHint
	{
		/// <summary>
		/// Hints the availability of performance mode.
		/// Allows scene to pre-allocate required buffers.
		/// </summary>
		MESH_UPDATE_MODE_HINT_PERFORMANCE_BIT = 1 << 0,
		/// <summary>
		/// Hints the availability of balanced mode.
		/// Allows scene to pre-allocate required buffers.
		/// </summary>
		MESH_UPDATE_MODE_HINT_BALANCED_BIT = 1 << 1,
		/// <summary>
        /// Hints the availability of memory saving mode.
		/// Allows scene to pre-allocate required buffers.
		/// </summary>
		MESH_UPDATE_MODE_HINT_MEMORY_SAVING_BIT = 1 << 2,
		/// <summary>
		/// Hints the availability of all the updates modes
		/// Allows scene to pre-allocate required buffers.
		/// </summary>
		MESH_UPDATE_MODE_HINT_ALL_BIT = MESH_UPDATE_MODE_HINT_PERFORMANCE_BIT | MESH_UPDATE_MODE_HINT_BALANCED_BIT | MESH_UPDATE_MODE_HINT_MEMORY_SAVING_BIT
	};

	template<uint32_t OffsetCount>
	struct MeshDrawArenaBufferGroup 
	{
		VKCORE::Buffer VertexBuffer;
		VKCORE::Buffer IndexBuffer;
		VKCORE::Buffer IndirectBuffer;

		size_t VertexBufferSize;
		size_t IndexBufferSize;
		size_t IndirectBufferSize;

		size_t VertexOffset[OffsetCount];
		size_t IndexOffset[OffsetCount];
		size_t IndirectOffset[OffsetCount];

		void Destroy(VkDevice& LogicalDevice)
		{
			VertexBuffer.Destroy(LogicalDevice);
			IndexBuffer.Destroy(LogicalDevice);
			IndirectBuffer.Destroy(LogicalDevice);
		}
	};

	struct MeshDrawBufferGroup
	{
		std::vector<VKCORE::Buffer> VertexBuffers;
		std::vector<VKCORE::Buffer> IndexBuffers;
		std::vector<VKCORE::Buffer> IndirectBuffers;

		void Destroy(VkDevice& LogicalDevice)
		{
			for (size_t i = 0; i < VertexBuffers.size(); i++)
			{
				VertexBuffers[i].Destroy(LogicalDevice);
			}
			for (size_t i = 0; i < IndexBuffers.size(); i++)
			{
				IndexBuffers[i].Destroy(LogicalDevice);
			}
			for (size_t i = 0; i < IndirectBuffers.size(); i++)
			{
				IndirectBuffers[i].Destroy(LogicalDevice);
			}
		}
	};

	struct MeshDrawArenaBufferGroup<0>
	{
		VKCORE::Buffer VertexBuffer;
		VKCORE::Buffer IndexBuffer;
		VKCORE::Buffer IndirectBuffer;

		size_t VertexBufferSize;
		size_t IndexBufferSize;
		size_t IndirectBufferSize;

		void Destroy(VkDevice& LogicalDevice)
		{
			VertexBuffer.Destroy(LogicalDevice);
			IndexBuffer.Destroy(LogicalDevice);
			IndirectBuffer.Destroy(LogicalDevice);
		}
	};

	struct MeshDrawStagingBufferGroup
	{
		VKCORE::PersistentBuffer IndexBuffer;
		VKCORE::PersistentBuffer VertexBuffer;
		VKCORE::PersistentBuffer IndirectCommandBuffer;

		size_t VertexBufferSize;
		size_t IndexBufferSize;
		size_t IndirectBufferSize;

		void Destroy(VkDevice& LogicalDevice)
		{
			VertexBuffer.Buffer.Destroy(LogicalDevice);
			IndexBuffer.Buffer.Destroy(LogicalDevice);
			IndirectCommandBuffer.Buffer.Destroy(LogicalDevice);
		}
	};

	/// <summary>
	/// Represents a 3D scene containing entities and lights, and manages related GPU resources for rendering.
	/// </summary>
	class Scene
	{
		friend class VKAPP::Renderer;
		friend class ResourceDependencyManager;
	public:
		Scene(VKAPP::RendererContext& RendererContext, MeshUpdateModeHint MeshUpdateModeHint);
		Scene() = default;
		void Create(VKAPP::RendererContext& RendererContext, MeshUpdateModeHint MeshUpdateModeHint);
		void Destroy();

		std::unordered_map<uint64_t, Entity*> Entities;
		std::unordered_map<uint64_t, Light*> StaticLights;
		std::unordered_map<uint64_t, Light*> DynamicLights;

		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;
		void SetCubemap(Cubemap& DestinationCubeMap);
		void SetCamera(Camera3D &Camera);

		Cubemap* SceneCubeMap;
		Camera3D* Camera;

		template<typename... args>
		void RequestMultipleFramesUpdate(ScenePendingUpdateType UpdateType, SceneResourceUpdateCallback& UpdateCallback,args&&... UpdateFunctionArguments);
		void RequestSingleFrameUpdate(ScenePendingUpdateType UpdateType,uint32_t FrameIndex,SceneResourceUpdateCallback& UpdateCallback);

		void CreateLightBuffers(uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount);
		void UpdateDynamicLightBuffers();
		void UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame);
		
		void UpdateStaticLightBuffers();
		void UpdateStaticFrameLightBuffers(uint32_t CurrentFrame);
		
		void CreateMeshBuffers();
		void UpdateMeshBuffers();
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		void CreateMeshTextureDescriptors(uint32_t MaxTextures = 1000);
		uint32_t GetMaxTextureCount() { return ActualTextureUpperBound; };
		void DestroyMeshTextureDescriptors();
		void UpdateTextureDescriptors(VKSCENE::TextureImportManager& TextureImportManager);

		void DestroyMeshBuffers();
		void DestroyLightBuffers();

		void HandleUpdateRequests(uint32_t CurrentFrame);

		bool DrawCubeMap;
	private:
		MeshDrawArenaBufferGroup<MAX_FRAMES_IN_FLIGHT> PerformanceModeBuffers;
		MeshDrawBufferGroup BalancedModeBuffers;
		uint32_t CurrentBalancedBuffer = 0;
		MeshDrawArenaBufferGroup<0> MemorySavingModeBuffers;
		MeshDrawStagingBufferGroup MeshStagingBuffers;

		std::vector<VKCORE::PersistentBuffer> SceneModelMatricesBuffer{};
		std::vector<VKCORE::Buffer> SceneMeshIndexBuffers;

		MeshUpdateModeHint meshUpdateModeHint;

		void GetMeshBuffersSizes(
			std::vector<std::pair<VKSCENE::Mesh*, uint32_t>>& Meshes,
			uint32_t &TotalVertexBufferSize,
			uint32_t &TotalIndexBufferSize,
			uint32_t &TotalIndirectCommandBufferSize
		);
		void FillMeshesArray(std::vector<uint32_t>& ModelIndexes);

		void ProcessMeshes(
			std::vector<std::vector<VkDrawIndexedIndirectCommand>>& DrawCommands,
			std::vector<uint32_t>& ModelIndexes,
			VKSCENE::BatchInfo &PerformanceModelBatch,
			VKSCENE::BatchInfo &BalancedModelBatch,
			VKSCENE::BatchInfo &MemorySavingModelBatch
		);
		void BatchMeshes(
			std::vector<std::pair<VKSCENE::Mesh*, uint32_t>>& Meshes,
			VKSCENE::BatchInfo& DestinationBatch
		);


		void CreatePerformanceModeBuffers(
			VkDeviceSize PerformanceVertexBufferSize,
			VkDeviceSize PerformanceIndexBufferSize,
			VkDeviceSize PerformanceIndirectCommandBufferSize
		);

		void CopyDataIntoPerformanceModeBuffers(
			VkDeviceSize PerformanceVertexBufferSize,
			VkDeviceSize PerformanceIndexBufferSize,
			VkDeviceSize PerformanceIndirectCommandBufferSize
		);


		
		void CreateBalancedModeBuffers(
			VkDeviceSize BalancedVertexBufferSize,
			VkDeviceSize BalancedIndexBufferSize,
			VkDeviceSize BalancedIndirectCommandBufferSize
		);

		void CopyDataIntoBalancedModeBuffers(
			VkDeviceSize BalancedVertexBufferSize,
			VkDeviceSize BalancedIndexBufferSize,
			VkDeviceSize BalancedIndirectCommandBufferSize
		);



		void CreateMemorySavingModeBuffers(
			VkDeviceSize MemorySavingVertexBufferSize,
			VkDeviceSize MemorySavingIndexBufferSize,
			VkDeviceSize MemorySavingIndirectCommandBufferSize
		);

		void CopyDataIntoMemorySavingModeBuffers(
			VkDeviceSize MemorySavingVertexBufferSize,
			VkDeviceSize MemorySavingIndexBufferSize,
			VkDeviceSize MemorySavingIndirectCommandBufferSize
		);

		//std::vector<std::vector<std::pair<bool, SceneResourceUpdateCallback*>>> PendingResourceUpdates;
		std::vector<std::vector<std::pair<ScenePendingUpdateType, SceneResourceUpdateCallback*>>> PendingResourceUpdates;

		std::vector<VKCORE::PersistentBuffer> DynamicLightSSBO{};
		std::vector<VKCORE::Buffer> StaticLightSSBO{};
		VKCORE::PersistentBuffer StaticLightStagingBuffer{};

		VKCORE::DescriptorPool SceneDescriptorPool;
		std::vector<VkDescriptorSet> SceneDescriptorSets;

		std::vector<VKCORE::Buffer> TexturesIndexBuffers{};
		std::vector<VKCORE::Buffer> TexturesIndexStagingBuffers{};
		VKCORE::DescriptorSetLayout TexturesDescriptorSetLayout;
		VKCORE::DescriptorPool TexturesDescriptorPool;
		std::vector<VkDescriptorSet> TexturesDescriptorSets;
		VKCORE::GraphicsPipeline* CurrentGbufferPassPipeline = nullptr;
		uint32_t ActualTextureUpperBound;
		void WriteTexture(
			MaterialTextureType TextureType,
			VKSCENE::Mesh& Mesh,
			VKSCENE::TextureImportManager& TextureImportManager,
			std::vector<VKCORE::DescriptorSetWriteImage>& ImageWrites,
			std::vector<int>& TextureIndexes,
			uint32_t CurrentImageIndex
		);

		std::vector<VkDescriptorSet> IndirectDescriptorSets;

		uint32_t EnabledMeshCount;
		std::vector<std::vector<std::pair<VKSCENE::Mesh*,uint32_t>>> Meshes;

		VKAPP::RendererContext* RendererContext;
		VKSCENE::ResourceDependencyManager* DependencyManager = nullptr;
	};
}
