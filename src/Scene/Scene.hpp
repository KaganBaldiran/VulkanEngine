#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <future>
#include <queue>
#include <unordered_set>

#include "../Renderer/Core/VulkanBuffer.hpp"
#include "../Physics/DebugDrawer.hpp"
#include "../Renderer/Core/VulkanDescriptorPool.hpp"
#include "../Renderer/Core/VulkanDescriptorSet.hpp"
#include "../Renderer/Core/VulkanDescriptorSetLayout.hpp"
#include "../Renderer/Core/VulkanDescriptor.hpp"

#include "../Common/DestructionQueue.hpp"
#include "../Common/CommonDefinitions.hpp"

#include "Mesh.hpp"
#include "SceneMeshManager.hpp"
#include "SceneLightManager.hpp"
#include "Light.hpp"
#include "ModelInstance.hpp"
#include "MaterialManager.hpp"
#include "MeshManager.hpp"
#include "PersistentSceneStagingBuffer.hpp"

namespace RENDERER_CORE
{
	//Forward Declarations
	class Window;
	struct Buffer;
	class GraphicsPipeline;
}

namespace RENDERER
{
	class Renderer;
	class RendererContext;
	class DeferredRenderPipeline;
}

namespace SCENE
{
	//Forward Declarations
	class Camera3D;
	class Cubemap;
	class Resource;

	enum SceneDynamicUploadMode
	{
		SCENE_DYNAMIC_UPLOAD_MODE_AUTO = 0,
		SCENE_DYNAMIC_UPLOAD_MODE_HOST_VISIBLE = 1,
		SCENE_DYNAMIC_UPLOAD_MODE_DEVICE_LOCAL = 2
	};

	struct SceneOptions
	{
		//States the upload mode for buffers that are expected to be changed dynamically like model transformations. 
		//Host visible mode keeps the buffers visible to the host(the CPU) which grants faster writes in return of slower reads.
		//Might be preferable in case where scene has few instances linked. 
		//Device local mode keeps the buffers device(chosen graphics device, discrete GPU if available) visible only which grants slower writes (not necessarily) in return of higher reading speeds.
		//Especially preferable in cases where scene updates are few or linked instance count is high.
		//By default AUTO mode enforces device visible if a discrete device is available.
		SceneDynamicUploadMode UploadMode = SCENE_DYNAMIC_UPLOAD_MODE_DEVICE_LOCAL;
		//Setting stating whether scene should preallocate the first initial notch as much as buffer allocation step. 
		//In case of disabling this feature, scene will allocate the first step during the initial update flush.
		bool PreallocateInitialBufferStep = false;
		//Allocation step used for buffer growth. 
		//An overly large step may cause memory waste whereas a small step might cause unwanted frequent reallocations. 
		size_t BufferAllocationStep = RENDERER_CORE::MEMORY_SIZE_KILOBYTE * 10;
	};

	enum SceneUpdateType : uint32_t
	{
		SCENE_UPDATE_TYPE_NONE = 0,
		SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS = 1 << 1,
		SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS = 1 << 2,
		SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS = 1 << 3,
		SCENE_UPDATE_TYPE_LINK_MESHES = 1 << 4,
		SCENE_UPDATE_TYPE_UNLINK_MESHES = 1 << 5,
		SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS = 1 << 6,

		SCENE_UPDATE_TYPE_ALL = SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS |
										SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS |
										SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS |
										SCENE_UPDATE_TYPE_LINK_MESHES |
										SCENE_UPDATE_TYPE_UNLINK_MESHES |
										SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS,

		SCENE_UPDATE_TYPE_ALL_PENDING = std::numeric_limits<uint32_t>::max()
	};

	enum MarkChangedType
	{
		MARK_CHANGED_TYPE_NONE = 0,
		MARK_CHANGED_TYPE_STATIC_LIGHT = 1 << 1,
		MARK_CHANGED_TYPE_DYNAMIC_LIGHT = 1 << 2,
		MARK_CHANGED_TYPE_MESH_TRANSFORMATION = 1 << 3,
		MARK_CHANGED_TYPE_MESH_GEOMETRY = 1 << 4,
		MARK_CHANGED_TYPE_MESH_TEXTURE = 1 << 5
	};

	inline SceneUpdateType operator|(SceneUpdateType a, SceneUpdateType b)
	{
		return static_cast<SceneUpdateType>(static_cast<int>(a) | static_cast<int>(b));
	}

	//Represents a 3D scene containing entities and lights, and manages related GPU resources for rendering.
	// Scene doesn't hold the actual geometry(mesh) data, it merely holds the data that is required to use the existing geometries.
	// The data includes indirect draw commands, meta data (material and model matrix indexes),texture descriptor indexes and model matrixes
	// The linked resources don't actually know about the scene 
	class Scene : COMMON::Destructible
	{
		friend class RENDERER::Renderer;
		friend class ResourceDependencyManager;
		friend class MeshManager;
		friend class RENDERER::DeferredRenderPipeline;
	public:
		Scene(RENDERER::RendererContext& RendererContext,TextureManager& Manager, MeshManager& MeshManager, SceneOptions Options = SceneOptions());
		Scene() = default;
		void Create(RENDERER::RendererContext& RendererContext, TextureManager& Manager, MeshManager& MeshManager, SceneOptions Options = SceneOptions());
		void Destroy() override;

		void LinkModelInstance(ModelInstance &Instance);
		void LinkModelInstance(std::vector<ModelInstance*> &Instances);
		void UnlinkModelInstance(ModelInstance& Instance);
		void UnlinkModelInstance(std::vector<ModelInstance*>& Instances);
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		void LinkDynamicLight(Light& DynamicLight);
		void LinkStaticLight(Light& StaticLight);
		void LinkDynamicLight(std::vector<Light*>& DynamicLights);
		void LinkStaticLight(std::vector<Light*>& StaticLights);

		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;
		Cubemap* SceneCubeMap;
		Camera3D* Camera;

		void LinkCubemap(Cubemap& DestinationCubeMap);
		void LinkCamera(Camera3D &Camera);

		void DestroyMeshBuffers();

		void MarkResourceChanged(Resource* Resource, MarkChangedType Type, uint32_t FrameIndex);
		void FlushPendingUpdates(SceneUpdateType Type,uint32_t FrameIndex);

		bool DrawCubeMap;
	private:
		struct UpdateLists
		{
			std::vector<ModelInstance*> ModelInstancesAppendList;
			std::vector<ModelInstance*> ModelInstancesEraseList;
			std::vector<ModelInstance*> ModelInstancesTransformationUpdateList;
			std::vector<ModelInstance*> MaterialUpdateList;

			std::vector<Light*> DynamicLightAppendUpdateList;
			std::vector<Light*> StaticLightAppendUpdateList;
			std::vector<Light*> DynamicLightEraseList;
			std::vector<Light*> StaticLightEraseList;
		};

		SCENE::SceneMeshManager MeshBuffers;
		SCENE::LightManager LightManager;
		std::array<PersistentStagingBuffer, MAX_FRAMES_IN_FLIGHT> StagingBuffers;
		std::array<std::array<RENDERER_CORE::BufferCopyInfo, static_cast<int>(BUFFER_COPY_SLOT_SIZE)>, MAX_FRAMES_IN_FLIGHT> SceneCopyInfos;
		SceneOptions Options;

		std::array<UpdateLists, MAX_FRAMES_IN_FLIGHT> UpdateLists;
		std::array<SceneUpdateType, MAX_FRAMES_IN_FLIGHT> PendingUpdateBits;

		RENDERER_CORE::DescriptorPool SceneDescriptorPool;
		std::array<VkDescriptorSet,MAX_FRAMES_IN_FLIGHT> SceneDescriptorSets;
	
		// 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Texture index buffers accessed from G-buffer pass fragment shader
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> TextureIndicesDescriptorSets;		
		std::array<VkDescriptorSet,MAX_FRAMES_IN_FLIGHT> IndirectDescriptorSets;

		//Links to the managers
		RENDERER::RendererContext* RendererContext = nullptr;
		SCENE::ResourceDependencyManager* DependencyManager = nullptr;
		TextureManager* TextureManager = nullptr;
		MeshManager* MeshManagerPtr = nullptr;
	};
}
