#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <map>
#include <future>
#include <queue>
#include <set>

namespace COMMON
{
    uint64_t GenerateHandleID();
    class Handle  
    {  
    public:  
        Handle() : HandleID(GenerateHandleID()) {}
        virtual ~Handle() = default;  
        uint64_t GetHandleID() const { return HandleID; }

        Handle(const Handle&) : HandleID(GenerateHandleID()) {}
        Handle(Handle&& Other) noexcept : HandleID(Other.HandleID) {}

        Handle& operator=(const Handle& Other)
        {
            return *this;
        }
        Handle& operator=(const Handle&& Other) noexcept
        {
            return *this;
        }
    protected:
        uint64_t HandleID;  
    };
}