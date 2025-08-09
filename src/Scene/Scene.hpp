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
#include "../Common/CommonDefinitions.hpp"
#include "../Renderer/Core/VulkanDescriptor.hpp"

#include "Mesh.hpp"
#include "SceneMeshManager.hpp"
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
	class SceneResource;
	class TextureImportManager;

	enum SceneUpdateType
	{
		SCENE_UPDATE_TYPE_NONE = 0,
		SCENE_UPDATE_TYPE_UPDATE_STATIC_LIGHT_BUFFERS = 1 << 1,
		SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS = 1 << 2,
		SCENE_UPDATE_TYPE_UPDATE_TEXTURE_DESCRIPTORS = 1 << 3,
		SCENE_UPDATE_TYPE_LINK_MESHES = 1 << 4,
		SCENE_UPDATE_TYPE_UNLINK_MESHES = 1 << 5,
		SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS = 1 << 6	
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
	class Scene
	{
		friend class RENDERER::Renderer;
		friend class ResourceDependencyManager;
		friend class MeshManager;
	public:
		Scene(RENDERER::RendererContext& RendererContext,TextureImportManager& Manager);
		Scene() = default;
		void Create(RENDERER::RendererContext& RendererContext, TextureImportManager& Manager);
		void Destroy();

		std::array<std::unordered_set<ModelInstance*>,MAX_FRAMES_IN_FLIGHT> ModelInstances;
		std::array<std::vector<ModelInstance*>,MAX_FRAMES_IN_FLIGHT> ModelInstancesAppendList;
		std::array<std::vector<ModelInstance*>,MAX_FRAMES_IN_FLIGHT> ModelInstancesEraseList;
		std::array<std::vector<ModelInstance*>,MAX_FRAMES_IN_FLIGHT> ModelInstancesTransformationUpdateList;

		void LinkModelInstance(ModelInstance &Instance);
		void LinkModelInstance(std::vector<ModelInstance*> &Instances);
		void UnlinkModelInstance(ModelInstance& Instance);
		void UnlinkModelInstance(std::vector<ModelInstance*>& Instances);
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		std::unordered_map<uint64_t, Light*> StaticLights;
		std::unordered_map<uint64_t, Light*> DynamicLights;

		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;

		Cubemap* SceneCubeMap;
		Camera3D* Camera;

		void SetCubemap(Cubemap& DestinationCubeMap);
		void SetCamera(Camera3D &Camera);

		void CreateLightBuffers(uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount);
		void UpdateDynamicLightBuffers();
		void UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame);
		
		void UpdateStaticLightBuffers();
		void UpdateStaticFrameLightBuffers(uint32_t CurrentFrame);
		

		void CreateMeshTextureDescriptors(uint32_t MaxTextures = 1000);
		uint32_t GetMaxTextureCount() { return ActualTextureUpperBound; };
		void DestroyMeshTextureDescriptors();

		void DestroyMeshBuffers();
		void DestroyLightBuffers();

		void MarkResourceChanged(SceneResource* Resource, MarkChangedType Type, uint32_t FrameIndex);
		void FlushPendingUpdates(SceneUpdateType Type,uint32_t FrameIndex);

		bool DrawCubeMap;
	private:
		SCENE::MeshManager MeshBuffers;

		std::vector<RENDERER_CORE::PersistentBuffer> DynamicLightSSBO{};
		std::vector<RENDERER_CORE::Buffer> StaticLightSSBO{};
		RENDERER_CORE::PersistentBuffer StaticLightStagingBuffer{};

		RENDERER_CORE::DescriptorPool SceneDescriptorPool;
		std::vector<VkDescriptorSet> SceneDescriptorSets;

		std::vector<RENDERER_CORE::Buffer> TexturesIndexBuffers{};
		std::vector<RENDERER_CORE::Buffer> TexturesIndexStagingBuffers{};
		
		// 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER - Mesh texture array to be used in G-buffer fragment shader
		// 1: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER - Texture index buffers accessed from G-buffer pass fragment shader
		RENDERER_CORE::Descriptor<MAX_FRAMES_IN_FLIGHT> MeshTexturesDescriptor;

		RENDERER_CORE::GraphicsPipeline* CurrentGbufferPassPipeline = nullptr;
		uint32_t ActualTextureUpperBound;
		
		std::vector<VkDescriptorSet> IndirectDescriptorSets;

		RENDERER::RendererContext* RendererContext = nullptr;
		SCENE::ResourceDependencyManager* DependencyManager = nullptr;
		TextureImportManager* TextureManager = nullptr;
	};
}
