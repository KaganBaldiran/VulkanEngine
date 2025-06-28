#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <map>
#include <future>
#include <queue>
#include <unordered_set>
#include "Mesh.hpp"

#include "Light.hpp"
#include "Scene.hpp"
#include "SceneResource.hpp"

#include <bitset>

namespace VKAPP
{
	class Renderer;
	class RendererContext;
}

namespace VKSCENE
{
	enum ResourceLinkingFlags
	{
		RESOURCE_LINKING_FLAG_NONE = 0,
		RESOURCE_LINKING_FLAG_SET_LIGHT_DYNAMIC = 1 << 0,
		RESOURCE_LINKING_FLAG_SET_LIGHT_STATIC = 1 << 1,

		RESOURCE_LINKING_FLAG_ALL = RESOURCE_LINKING_FLAG_SET_LIGHT_DYNAMIC | RESOURCE_LINKING_FLAG_SET_LIGHT_STATIC
	};

	/// <summary>
	/// Manages dependencies between scene resources and scenes, allowing linking, unlinking, and updating of resource-scene relationships.
	/// </summary>
	class ResourceDependencyManager
	{
		friend class SceneResource;
		friend class Scene;
	public:
		ResourceDependencyManager(VKAPP::RendererContext& rendererContext);
		ResourceDependencyManager() = default;
		void Create(VKAPP::RendererContext& rendererContext);

		void RegisterResource(SceneResource& Resource);

		void LinkSceneResource(SceneResource& Resource, Scene& DestinationScene,ResourceLinkingFlags Flags = RESOURCE_LINKING_FLAG_NONE);
		void UnlinkSceneResource(SceneResource& Resource, Scene& DestinationScene);
		void UpdateDependencies();

		/// <summary>
		/// Retrieves the linked SceneResource associated with the specified name.
		/// </summary>
		/// <param name="Name">The name of the resource to retrieve.</param>
		/// <returns>A pointer to the linked SceneResource if found; otherwise, nullptr.</returns>
		SceneResource* GetLinkedResource(std::string Name);
		/// <summary>
		/// Retrieves the linked SceneResource associated with the given ID.
		/// </summary>
		/// <param name="ID">The unique identifier of the linked resource to retrieve.</param>
		/// <returns>A pointer to the linked SceneResource if found; otherwise, nullptr.</returns>
		SceneResource* GetLinkedResource(uint64_t ID);
		/// <summary>
		/// Retrieves the set of scenes that are linked to the specified scene resource.
		/// </summary>
		/// <param name="Resource">A reference to the SceneResource whose linked scenes are to be retrieved.</param>
		/// <returns>A set containing pointers to Scene objects that are linked to the given resource.</returns>
		std::set<Scene*> GetLinkedResourceScenes(SceneResource &Resource);
	private:
		void MarkResourceDirty(SceneResource &Resource);

		std::unordered_map<SceneResource*, std::set<Scene*>> ResourceSceneLinks;
		std::unordered_map<std::string,SceneResource*> NameResourceLinks;
		std::unordered_map<uint64_t,SceneResource*> IDResourceLinks;

		std::unordered_map<SceneResource*,ResourceLinkingFlags> LinkingFlags;

		VKAPP::RendererContext* rendererContext;
		std::bitset<16384> DirtyResourceFlags;
	};
}
