#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <map>
#include <future>
#include <queue>
#include <set>

namespace SCENE
{
	enum ResourceType
	{
		RESOURCE_TYPE_UNSPECIFIED = 0,
		RESOURCE_TYPE_LIGHT = 1,
		RESOURCE_TYPE_CAMERA = 2,
		RESOURCE_TYPE_CUBEMAP = 3,
		RESOURCE_TYPE_ENTITY = 4
	};

    uint64_t GenerateResourceID();

    class Resource  
    {  
    public:  
        Resource();  
        virtual ~Resource() = default;  

        ResourceType resourceType = RESOURCE_TYPE_UNSPECIFIED;  
        const uint64_t ResourceID;  
    protected:  
         
    };
}