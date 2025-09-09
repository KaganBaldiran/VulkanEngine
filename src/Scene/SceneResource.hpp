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
    uint64_t GenerateResourceID();

    class Resource  
    {  
    public:  
        Resource();  
        virtual ~Resource() = default;  
        const uint64_t ResourceID;  
    protected:  
         
    };
}