#pragma once
#include "VulkanShader.hpp"

#include <map>
#include <string>

namespace RENDERER_CORE
{
	class ShaderManager
	{
	public:
		void Destroy(VkDevice LogicalDevice);
		RENDERER_CORE::ShaderModule* AppendShaderModule(
			const char* ShaderLabel,
			const char* GLSLSourceFileName,
			const char* SpirvSourceFileName,
			shaderc_shader_kind ShaderType,
			VkDevice LogicalDevice
		);
		RENDERER_CORE::ShaderModule* AppendShaderModule(const char* ShaderLabel, const RENDERER_CORE::ShaderModule& Module);
		RENDERER_CORE::ShaderModule* AppendShaderModule(const char* ShaderLabel,RENDERER_CORE::ShaderData &Data, VkDevice LogicalDevice);
		void EraseShaderModule(std::string ShaderLabel, VkDevice LogicalDevice);
		//Returns a pointer to the shader module retrieved with the given key.
		//Returns nullptr if the element with the given key doesn't exist.
		RENDERER_CORE::ShaderModule* GetShaderModule(std::string ShaderLabel);
	private:
		std::map<std::string, RENDERER_CORE::ShaderModule> ShaderModules;
	};
}