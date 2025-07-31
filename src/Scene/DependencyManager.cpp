#include "DependencyManager.hpp"
#include "Camera.hpp"
#include "Cubemap.hpp"
#include "Light.hpp"

VKSCENE::ResourceDependencyManager::ResourceDependencyManager(VKAPP::RendererContext& rendererContext)
{
	Create(rendererContext);
}

void VKSCENE::ResourceDependencyManager::Create(VKAPP::RendererContext& rendererContext)
{
	this->rendererContext = &rendererContext;
}

void VKSCENE::ResourceDependencyManager::RegisterResource(SceneResource& Resource)
{
	Resource.dependencyManager = this;
}

void VKSCENE::ResourceDependencyManager::LinkSceneResource(SceneResource& Resource, Scene& DestinationScene, ResourceLinkingFlags Flags)
{
	if (!Resource.dependencyManager) Resource.dependencyManager = this;
	if (!DestinationScene.DependencyManager) DestinationScene.DependencyManager = this;
	
	auto& Scenes = this->ResourceSceneLinks[&Resource];
	Scenes.emplace(&DestinationScene);
	NameResourceLinks[Resource.Name] = &Resource;
	IDResourceLinks[Resource.ResourceID] = &Resource;

	switch (Resource.resourceType)
	{
	case RESOURCE_TYPE_CAMERA:
	{   
		if (Camera3D* Camera = dynamic_cast<Camera3D*>(&Resource))
		{
		   DestinationScene.Camera = Camera;
		}
		break;
	}
	case RESOURCE_TYPE_CUBEMAP:
	{
		if (Cubemap* cubeMap = dynamic_cast<Cubemap*>(&Resource))
		{
			DestinationScene.SetCubemap(*cubeMap);
		}
		break;
	}
	case RESOURCE_TYPE_LIGHT:
	{
		if (Flags & RESOURCE_LINKING_FLAG_SET_LIGHT_DYNAMIC)
		{
			if (Light* DynamicLight = dynamic_cast<Light*>(&Resource))
			{
				DestinationScene.DynamicLights.emplace(DynamicLight->ResourceID,DynamicLight);
			}
		}
		else if (Flags & RESOURCE_LINKING_FLAG_SET_LIGHT_STATIC)
		{
			if (Light* StaticLight = dynamic_cast<Light*>(&Resource))
			{
				DestinationScene.StaticLights.emplace(StaticLight->ResourceID,StaticLight);
			}
		}
		else
		{
			throw std::runtime_error("Error while linking a light resource! No appropriate linking flag provided!");
		}
		break;
	}
	case RESOURCE_TYPE_UNSPECIFIED:
	{
		throw std::runtime_error("A resource with unspecified type is provided! Specify the resource type to continue linking!");
		break;
	}

	default:
		break;
	}

	LinkingFlags[&Resource] = Flags;
}

void VKSCENE::ResourceDependencyManager::UnlinkSceneResource(SceneResource& Resource, Scene& DestinationScene)
{
	auto& Scenes = this->ResourceSceneLinks[&Resource];
	auto SceneIterator = Scenes.find(&DestinationScene);
	if (SceneIterator == Scenes.end()) {
		throw std::runtime_error("Given resource is not linked on the given scene!");
	}

	switch (Resource.resourceType)
	{
	case RESOURCE_TYPE_CAMERA:
	{
		std::cerr << "Singular resource types like camera cannot be unlinked! Try linking another camera instead!" << std::endl;
		return;
		break;
	}
	case RESOURCE_TYPE_CUBEMAP:
	{
		std::cerr << "Singular resource types like cubemap cannot be unlinked! Try linking another cubemap instead!" << std::endl;
		return;
		break;
	}
	case RESOURCE_TYPE_LIGHT:
	{
		ResourceLinkingFlags Flag;
		auto LinkingFlagsIterator = LinkingFlags.find(&Resource);
		if (LinkingFlagsIterator != LinkingFlags.end()) {
			Flag = LinkingFlagsIterator->second;
		}
		else Flag = RESOURCE_LINKING_FLAG_NONE;

		if (Flag & RESOURCE_LINKING_FLAG_SET_LIGHT_DYNAMIC)
		{
			DestinationScene.DynamicLights.erase(Resource.ResourceID);	
		}
		else if (Flag & RESOURCE_LINKING_FLAG_SET_LIGHT_STATIC)
		{
			DestinationScene.StaticLights.erase(Resource.ResourceID);
		}
		break;
	}
	case RESOURCE_TYPE_UNSPECIFIED:
	{
		throw std::runtime_error("A resource with unspecified type is provided! Specify the resource type to continue linking!");
		break;
	}

	default:
		break;
	}

	Scenes.erase(&DestinationScene);
	if (Scenes.empty()) {
		ResourceSceneLinks.erase(&Resource);
		NameResourceLinks.erase(Resource.Name);
		IDResourceLinks.erase(Resource.ResourceID);
		LinkingFlags.erase(&Resource);
	}
}

void VKSCENE::ResourceDependencyManager::UpdateDependencies()
{
	if(DirtyResourceFlags.any()) DirtyResourceFlags.reset();
}

VKSCENE::SceneResource* VKSCENE::ResourceDependencyManager::GetLinkedResource(std::string Name)
{
	auto Iterator = NameResourceLinks.find(Name);
	if (Iterator != NameResourceLinks.end()) {
		return Iterator->second;
	}
	return nullptr;
}

VKSCENE::SceneResource* VKSCENE::ResourceDependencyManager::GetLinkedResource(uint64_t ID)
{
	auto Iterator = IDResourceLinks.find(ID);
	if (Iterator != IDResourceLinks.end()) {
		return Iterator->second;
	}
	return nullptr;
}

std::set<VKSCENE::Scene*> VKSCENE::ResourceDependencyManager::GetLinkedResourceScenes(SceneResource& Resource)
{
	auto Iterator = ResourceSceneLinks.find(&Resource);
	if (Iterator != ResourceSceneLinks.end()) {
		return Iterator->second;
	}
	return std::set<Scene*>();
}

void VKSCENE::ResourceDependencyManager::MarkResourceDirty(SceneResource& Resource)
{
	DirtyResourceFlags.set(Resource.ResourceID, true);
}

bool VKSCENE::ResourceDependencyManager::IsResourceDirty(uint64_t ResourceID)
{
	return static_cast<bool>(DirtyResourceFlags[ResourceID]);
}


