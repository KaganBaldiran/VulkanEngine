#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>

namespace VKCORE
{
	std::vector<char> ReadFile(const char* FileName);
	int CompileGLSL(const std::string& SourceFileName, const std::string& DestinationFileName);
	VkShaderModule CreateModule(const std::vector<char>& CodeSource, VkDevice& LogicalDevice);

	class ShaderModule
	{
	public:
		ShaderModule(const char* FileName, const char* SpirvFileName,bool CompileShaderIntoSpirv,VkDevice& LogicalDevice);
		ShaderModule() = default;
		void Create(const char* FileName, const char* SpirvFileName, bool CompileShaderIntoSpirv, VkDevice& LogicalDevice);
		void Destroy(VkDevice& LogicalDevice);
		VkShaderModule Module;
	};
}
