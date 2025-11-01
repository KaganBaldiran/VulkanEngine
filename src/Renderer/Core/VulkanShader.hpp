#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <vector>
#include "shaderc/shaderc.hpp"

namespace RENDERER_CORE
{
	std::vector<char> ReadFile(const char* FileName);
	std::string ReadFileStr(const char* FileName);
	void WriteFile(const char* FileName,std::string Content);
	void WriteFileSpirv(const char* FileName,std::vector<uint32_t> SpirvData);
	std::vector<uint32_t> CompileGLSL(const std::string& Source, shaderc_shader_kind ShaderKind, const char* Label);
	VkShaderModule CreateModule(const std::vector<uint32_t>& CodeSource, VkDevice& LogicalDevice);
	VkShaderModule CreateModule(const std::vector<char>& CodeSource, VkDevice& LogicalDevice);

	class ShaderData
	{
		friend class ShaderModule;
	public:
		void FromGLSL(const char* FileName, shaderc_shader_kind ShaderKind, const char* Label);
		void FromGLSL(std::string GLSLsource, shaderc_shader_kind ShaderKind, const char* Label);
		void FromSpirV(const std::vector<uint32_t> &Data);
		void FromSpirV(const char* FileName);
		void WriteFileSpirv(const char* FileName);
		size_t HashShaderData();
	private:
		std::vector<uint32_t> SpirvData;
	};

	class ShaderModule
	{
	public:
		ShaderModule(ShaderData& ShaderData, const char* ShaderLabel, VkDevice LogicalDevice);
		ShaderModule() = default;
		void Create(ShaderData& ShaderData, const char* ShaderLabel, VkDevice LogicalDevice);
		void CompileOrLoadShaderModule(
			const char* GLSLSourceFileName,
			const char* SpirvSourceFileName,
			shaderc_shader_kind ShaderType,
			const char* ShaderLabel,
			VkDevice LogicalDevice
		);
		void Destroy(VkDevice& LogicalDevice);
		bool operator==(const ShaderModule& Other) const;

		VkShaderModule Handle = VK_NULL_HANDLE;
		std::string Label;
		size_t SPIRV_Hash = 0;
	private:
	};
}
