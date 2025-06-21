#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <string>
#include <iostream>
#include <stdexcept>

namespace VKCORE
{
	struct VulkanResult
	{
		VkResult Flag;
		std::string Message;

		bool Success() { return Flag == VK_SUCCESS; };
	};

#define VULKAN_SUCCESS VKCORE::VulkanResult{VK_SUCCESS,"Function executed with success!"}
#define VULKAN_ASSERT_RESULT(FUNC) \
	{   \
		VKCORE::VulkanResult Result = FUNC;  \
        if(!Result.Success())   \
		{   \
			throw std::runtime_error(Result.Message); \
		}   \
	}   
#define VULKAN_CHECK_RESULT(FUNC) \
	{   \
		VKCORE::VulkanResult Result = FUNC;  \
        if(!Result.Success())   \
		{   \
			std::cerr << Result.Message << std::endl; \
            return Result; \
		}   \
	}   
}