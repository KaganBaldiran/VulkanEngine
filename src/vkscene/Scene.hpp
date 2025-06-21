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

	struct ModelImportInfo
	{
		VKSCENE::Model3D* DestinationModel;
		const char* ModelFilePath;
	};

	struct MeshImporter
	{
		void AppendImportTask(ModelImportInfo ImportInfo);
		void SubmitImport();
		void WaitImportIdle();
		std::vector<std::future<void>> Futures;
		std::queue<ModelImportInfo> ImportQueue;
		double StartingTime;
	};

	class Entity
	{
	public:
		std::string Name;
		VKSCENE::Model3D Model;
	private:
	};

	/// <summary>
	/// Represents a 3D scene containing entities and lights, and manages related GPU resources for rendering.
	/// </summary>
	class Scene
	{
		friend class VKAPP::Renderer;
	public:
		Scene() = default;
		Scene(VKAPP::RendererContext& RendererContext);
		void Create(VKAPP::RendererContext& RendererContext);
		void Destroy(VKAPP::RendererContext& RendererContext);

		std::vector<Entity*> Entities;
		std::vector<Light*> StaticLights;
		std::vector<Light*> DynamicLights;
		VKPHYSICS::DebugDrawer* DebugDrawer = nullptr;

		void UpdateDynamicLightBuffers();
		void UpdateDynamicFrameLightBuffers(uint32_t CurrentFrame);
		void UpdateStaticLightBuffers(VKAPP::RendererContext& RendererContext);
		void UpdateStaticFrameLightBuffers(VKAPP::RendererContext& RendererContext, uint32_t CurrentFrame);
		void CreateMeshBuffers(VKAPP::RendererContext &RendererContext);
		void UpdateMeshBuffers(VKAPP::RendererContext& RendererContext);
		void CreateLightBuffers(VKAPP::RendererContext &RendererContext,uint32_t MaxStaticLightCount, uint32_t MaxDynamicLightCount);
		void DestroyMeshBuffers(VKAPP::RendererContext& RendererContext);
		void DestroyLightBuffers(VKAPP::RendererContext& RendererContext);

	private:
		VKCORE::Buffer SceneIndexBuffer{};
		VKCORE::Buffer SceneVertexBuffer{};

		std::vector<VKCORE::PersistentBuffer> DynamicLightSSBO{};
		std::vector<VKCORE::Buffer> StaticLightSSBO{};
		VKCORE::PersistentBuffer StaticLightStagingBuffer{};

		VKCORE::DescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> DescriptorSets;
	};
}
