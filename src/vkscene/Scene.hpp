#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"

namespace VKCORE
{
	//Forward Declarations
	class Window;
	struct Buffer;
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

	class Scene
	{
	public:
		VKCORE::Buffer SceneVertexBuffer{};
		VKCORE::Buffer SceneIndexBuffer{};
		std::vector<Entity*> Entities;
		Camera3D* Camera;
	private:
	};
}
