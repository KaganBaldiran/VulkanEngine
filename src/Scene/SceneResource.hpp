#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <map>
#include <future>
#include <queue>
#include <set>

namespace VKSCENE
{
	//Forward Declarations
	class ResourceDependencyManager;

	enum ResourceType
	{
		RESOURCE_TYPE_UNSPECIFIED = 0,
		RESOURCE_TYPE_LIGHT = 1,
		RESOURCE_TYPE_CAMERA = 2,
		RESOURCE_TYPE_CUBEMAP = 3,
		RESOURCE_TYPE_ENTITY = 4
	};

    class SceneResource  
    {  
        friend class ResourceDependencyManager;  
    public:  
        SceneResource();  
        virtual ~SceneResource() = default;  

        ResourceType resourceType = RESOURCE_TYPE_UNSPECIFIED;  
        std::string Name;  
        const uint64_t ResourceID;  

        void SetDirty();  
    protected:  
        ResourceDependencyManager* dependencyManager = nullptr;  
         
        static uint64_t GenerateResourceID();  
    };
}