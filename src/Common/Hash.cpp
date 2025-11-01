#include "Hash.hpp"
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

#include <iostream>

size_t COMMON::HashNextPtrChain(void* Next)
{
    size_t ResultingHash = 21;
    auto* BasePtr = reinterpret_cast<const VkBaseInStructure*>(Next);

    while (BasePtr)
    {
        ResultingHash = CombineHash(ResultingHash,std::hash<VkStructureType>()(BasePtr->sType));
        switch (BasePtr->sType)
        {
        case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
        {
            auto* Ptr = reinterpret_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(BasePtr);
            ResultingHash = CombineHash(ResultingHash, std::hash<uint32_t>()(Ptr->bindingCount));
            for (uint32_t i = 0; i < Ptr->bindingCount; i++)
            {
                ResultingHash = CombineHash(ResultingHash, std::hash<VkDescriptorBindingFlags>()(Ptr->pBindingFlags[i]));
            }
            break;
        }
        default:
            ResultingHash = CombineHash(ResultingHash, std::hash<void*>()(Next));
            break;
        }
        BasePtr = BasePtr->pNext;
    }
    return ResultingHash;
}
