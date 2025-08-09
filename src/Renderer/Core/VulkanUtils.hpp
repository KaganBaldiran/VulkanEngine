#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <string>
#include <iostream>
#include <stdexcept>

namespace RENDERER_CORE
{
	struct VulkanResult
	{
		VkResult Flag;
		std::string Message;

		bool Success() { return Flag == VK_SUCCESS; };
	};

#define VULKAN_SUCCESS RENDERER_CORE::VulkanResult{VK_SUCCESS,"Function executed with success!"}
#define VULKAN_ASSERT_RESULT(FUNC) \
	{   \
		RENDERER_CORE::VulkanResult Result = FUNC;  \
        if(!Result.Success())   \
		{   \
			throw std::runtime_error(Result.Message); \
		}   \
	}   
#define VULKAN_CHECK_RESULT(FUNC) \
	{   \
		RENDERER_CORE::VulkanResult Result = FUNC;  \
        if(!Result.Success())   \
		{   \
			std::cerr << Result.Message << std::endl; \
            return Result; \
		}   \
	}   
}