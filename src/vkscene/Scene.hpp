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

	/// <summary>
	/// Represents a 3D scene containing entities and lights, and manages related GPU resources for rendering.
	/// </summary>
	class Scene
	{
		friend class VKAPP::Renderer;
		friend class ResourceDependencyManager;
	public:
		Scene(VKAPP::RendererContext& RendererContext);
		Scene() = default;
		void Create(VKAPP::RendererContext& RendererContext);
		void Destroy();

		//std::vector<Entity*> Entities;
		//std::vector<Light*> StaticLights;
		//std::vector<Light*> DynamicLights;

		std::unordered_map<uint64_t, Entity*> Entities;
		std::unordered_map<uint64_t, Light*> StaticLights;
		std::unordered_map<uint64_t, Light*> DynamicLights;

		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;
		void SetCubemap(Cubemap& DestinationCubeMap);
		void SetCamera(Camera3D &Camera);

		Cubemap* SceneCubeMap;
		Camera3D* Camera;

		void UpdateDynamicLightBuffers();
		void UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame);
		void UpdateStaticLightBuffers();
		void UpdateStaticFrameLightBuffers(uint32_t CurrentFrame);
		void CreateMeshBuffers();
		void UpdateMeshBuffers();
		void CreateLightBuffers(uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount);
		void DestroyMeshBuffers();
		void DestroyLightBuffers();
		void UpdateMeshTransformations(uint32_t CurrentFrame);

		bool DrawCubeMap;
	private:
		VKCORE::Buffer SceneIndexBuffer{};
		VKCORE::Buffer SceneVertexBuffer{};
		VKCORE::Buffer SceneIndirectCommandBuffer{};
		std::vector<VKCORE::PersistentBuffer> SceneModelMatricesBuffer{};

		std::vector<VKCORE::PersistentBuffer> DynamicLightSSBO{};
		std::vector<VKCORE::Buffer> StaticLightSSBO{};
		VKCORE::PersistentBuffer StaticLightStagingBuffer{};

		VKCORE::DescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> SceneDescriptorSets;

		std::vector<VkDescriptorSet> IndirectDescriptorSets;

		uint32_t EnabledMeshCount;
		std::vector<VKSCENE::Model3D*> Models;

		VKAPP::RendererContext* RendererContext;
		VKSCENE::ResourceDependencyManager* DependencyManager = nullptr;
	};
}
