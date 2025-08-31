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
}

namespace SCENE
{
	//Forward Declarations
	class Camera3D;
	class Cubemap;
	class Resource;
	class TextureImportManager;
	class MeshManager;

	enum SceneUpdateType
	{
		SCENE_UPDATE_TYPE_NONE = 0,
		SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS = 1 << 1,
		SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS = 1 << 2,
		SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS = 1 << 3,
		SCENE_UPDATE_TYPE_LINK_MESHES = 1 << 4,
		SCENE_UPDATE_TYPE_UNLINK_MESHES = 1 << 5,
		SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS = 1 << 6,

		SCENE_UPDATE_TYPE_ALL_PENDING = SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS |
										SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS |
										SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS |
										SCENE_UPDATE_TYPE_LINK_MESHES |
										SCENE_UPDATE_TYPE_UNLINK_MESHES |
										SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS
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

	using SceneCommandCallback = bool;

	/// <summary>
	/// Represents a 3D scene containing entities and lights, and manages related GPU resources for rendering.
	/// </summary>
	class Scene : COMMON::Destructible
	{
		friend class RENDERER::Renderer;
		friend class ResourceDependencyManager;
		friend class MeshManager;
	public:
		Scene(RENDERER::RendererContext& RendererContext,TextureImportManager& Manager, MeshManager& MeshManager);
		Scene() = default;
		void Create(RENDERER::RendererContext& RendererContext, TextureImportManager& Manager, MeshManager& MeshManager);
		void Destroy() override;

		std::array<std::unordered_set<ModelInstance*>,MAX_FRAMES_IN_FLIGHT> ModelInstances;

		void LinkModelInstance(ModelInstance &Instance);
		void LinkModelInstance(std::vector<ModelInstance*> &Instances);
		void UnlinkModelInstance(ModelInstance& Instance);
		void UnlinkModelInstance(std::vector<ModelInstance*>& Instances);
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		void LinkDynamicLight(Light& DynamicLight);
		void LinkStaticLight(Light& StaticLight);
		void LinkDynamicLight(std::vector<Light*>& DynamicLights);
		void LinkStaticLight(std::vector<Light*>& StaticLights);

		std::unordered_map<uint64_t, Light*> StaticLights;
		std::unordered_map<uint64_t, Light*> DynamicLights;

		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;

		Cubemap* SceneCubeMap;
		Camera3D* Camera;

		void LinkCubemap(Cubemap& DestinationCubeMap);
		void LinkCamera(Camera3D &Camera);

		//void CreateMeshTextureDescriptors(uint32_t MaxTextures = 1000);
		//uint32_t GetMaxTextureCount() { return ActualTextureUpperBound; };
		//void DestroyMeshTextureDescriptors();

		void DestroyMeshBuffers();

		void MarkResourceChanged(Resource* Resource, MarkChangedType Type, uint32_t FrameIndex);
		void FlushPendingUpdates(SceneUpdateType Type,uint32_t FrameIndex);

		bool DrawCubeMap;
	private:
		//void CreateMeshTextureDescriptors(uint32_t DescriptorCount);

		SCENE::SceneMeshManager MeshBuffers;
		SCENE::LightManager LightManager;

		std::array<std::vector<ModelInstance*>, MAX_FRAMES_IN_FLIGHT> ModelInstancesAppendList;
		std::array<std::vector<ModelInstance*>, MAX_FRAMES_IN_FLIGHT> ModelInstancesEraseList;
		std::array<std::vector<ModelInstance*>, MAX_FRAMES_IN_FLIGHT> ModelInstancesTransformationUpdateList;

		std::array<std::vector<Light*>, MAX_FRAMES_IN_FLIGHT> DynamicLightAppendUpdateList;
		std::array<std::vector<Light*>, MAX_FRAMES_IN_FLIGHT> StaticLightAppendUpdateList;
		std::array<std::vector<Light*>, MAX_FRAMES_IN_FLIGHT> DynamicLightEraseList;
		std::array<std::vector<Light*>, MAX_FRAMES_IN_FLIGHT> StaticLightEraseList;

		std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> PendingUpdateBits;

		RENDERER_CORE::DescriptorPool SceneDescriptorPool;
		std::vector<VkDescriptorSet> SceneDescriptorSets;
	
		// 0: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Texture index buffers accessed from G-buffer pass fragment shader
		std::vector<VkDescriptorSet> TextureIndicesDescriptorSets;

		//RENDERER_CORE::GraphicsPipeline* CurrentGbufferPassPipeline = nullptr;
		//uint32_t ActualTextureUpperBound;
		
		std::vector<VkDescriptorSet> IndirectDescriptorSets;

		RENDERER::RendererContext* RendererContext = nullptr;
		SCENE::ResourceDependencyManager* DependencyManager = nullptr;
		TextureImportManager* TextureManager = nullptr;
		MeshManager* MeshManagerPtr = nullptr;
	};
}
