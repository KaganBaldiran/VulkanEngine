#include "VulkanShader.hpp"
#include <fstream>
#include <sstream>

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include "../../Common/Hash.hpp"

VkShaderModule RENDERER_CORE::CreateModule(const std::vector<uint32_t>& CodeSource,VkDevice& LogicalDevice)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo{};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = CodeSource.size() * sizeof(uint32_t);
    ShaderModuleCreateInfo.pCode = CodeSource.data();

    VkShaderModule Module;
    if (vkCreateShaderModule(LogicalDevice, &ShaderModuleCreateInfo, nullptr, &Module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Shader module created [" + std::to_string(reinterpret_cast<uintptr_t>(Module)) + "]."));
    }
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Shader module created [" + std::to_string(reinterpret_cast<uintptr_t>(Module)) + "]."));
    return Module;
}

VkShaderModule RENDERER_CORE::CreateModule(const std::vector<char>& CodeSource, VkDevice& LogicalDevice)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo{};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = CodeSource.size();
    ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(CodeSource.data());

    VkShaderModule Module;
    if (vkCreateShaderModule(LogicalDevice, &ShaderModuleCreateInfo, nullptr, &Module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Shader module created [" + std::to_string(reinterpret_cast<uintptr_t>(Module)) + "]."));
    }
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Shader module created [" + std::to_string(reinterpret_cast<uintptr_t>(Module)) + "]."));
    return Module;
}

void RENDERER_CORE::ShaderModule::CompileOrLoadShaderModule(
    const char* GLSLSourceFileName,
    const char* SpirvSourceFileName,
    shaderc_shader_kind ShaderType,
    const char* ShaderLabel,
    VkDevice LogicalDevice
)
{
    if (!ShaderLabel)
    {
        throw std::runtime_error("Invalid shader label!");
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Given shader label is invalid. Each shader module must have a unique label!");
    }

    RENDERER_CORE::ShaderData ShaderData;
    if (VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS)
    {
        ShaderData.FromGLSL(GLSLSourceFileName, ShaderType, ShaderLabel);
        ShaderData.WriteFileSpirv(SpirvSourceFileName);
    }
    else ShaderData.FromSpirV(SpirvSourceFileName);
    this->Handle = CreateModule(ShaderData.SpirvData, LogicalDevice);
    Label = ShaderLabel;
    SPIRV_Hash = ShaderData.HashShaderData();
}

std::vector<char> RENDERER_CORE::ReadFile(const char* FileName)
{
    std::ifstream File(FileName, std::ios::ate | std::ios::binary);

    if (!File.is_open()) {
        throw std::runtime_error("Failed to open file (" + std::string(FileName) + ").");
    }

    size_t FileSize = static_cast<size_t>(File.tellg());
    std::vector<char> Buffer(FileSize);

    File.seekg(0);
    File.read(Buffer.data(), FileSize);
    File.close();
    return Buffer;
}

std::string RENDERER_CORE::ReadFileStr(const char* FileName)
{
    std::ifstream InputFile(FileName, std::ios::binary);
    std::stringstream Buffer;
    if (InputFile.is_open())
    {
        Buffer << InputFile.rdbuf();
        InputFile.close();
    }
    else
    {
        throw std::runtime_error("Failed to open file (" + std::string(FileName) + ").");
    }
    return Buffer.str();
}

void RENDERER_CORE::WriteFile(const char* FileName, std::string Content)
{
    std::ofstream File(FileName);

    if (File.is_open())
    {
        File << Content;
        File.close();
    }
    else
    {
        std::cout << "Unable to open file(" << FileName << ")";
    }
}

void RENDERER_CORE::WriteFileSpirv(const char* FileName, std::vector<uint32_t> SpirvData)
{
    std::ofstream File(FileName, std::ofstream::binary);
    if (!File.is_open()) {
        throw std::runtime_error("Failed to open file (" + std::string(FileName) + ").");
    }

    File.write(reinterpret_cast<const char*>(SpirvData.data()), SpirvData.size() * sizeof(uint32_t));
    File.close();
}

std::vector<uint32_t> RENDERER_CORE::CompileGLSL(const std::string& Source,shaderc_shader_kind ShaderKind,const char* Label)
{
    shaderc::Compiler Compiler;
    shaderc::CompileOptions Options;
    Options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto Result = Compiler.CompileGlslToSpv(Source, ShaderKind, Label, Options);

    if (Result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed to compile shader [" + std::string(Label) + "] with message:" +
            Result.GetErrorMessage() + "."));
        throw std::runtime_error("Failed to compile glsl into spir-v with message: " + Result.GetErrorMessage());
    }

    return { Result.cbegin(), Result.cend() };
}

RENDERER_CORE::ShaderModule::ShaderModule(ShaderData& ShaderData, const char* ShaderLabel, VkDevice LogicalDevice)
{
    Create(ShaderData, ShaderLabel, LogicalDevice);
}

void RENDERER_CORE::ShaderModule::Create(ShaderData& ShaderData, const char* ShaderLabel, VkDevice LogicalDevice)
{
    if (!ShaderLabel)
    {
        throw std::runtime_error("Invalid shader label!");
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Given shader label is invalid. Each shader module must have a unique label!");
    }
    this->Handle = CreateModule(ShaderData.SpirvData, LogicalDevice);
    SPIRV_Hash = ShaderData.HashShaderData();
    Label = ShaderLabel;
}

void RENDERER_CORE::ShaderModule::Destroy(VkDevice& LogicalDevice)
{
    if (Handle == VK_NULL_HANDLE) return;
    vkDestroyShaderModule(LogicalDevice, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
}

bool RENDERER_CORE::ShaderModule::operator==(const ShaderModule& Other) const
{
    return Other.Label == Label && Other.SPIRV_Hash == SPIRV_Hash;
}

void RENDERER_CORE::ShaderData::FromGLSL(const char* FileName, shaderc_shader_kind ShaderKind, const char* Label)
{
    std::string Source = ReadFileStr(FileName);
    SpirvData = CompileGLSL(Source, ShaderKind, Label);
}

void RENDERER_CORE::ShaderData::FromGLSL(std::string GLSLsource, shaderc_shader_kind ShaderKind, const char* Label)
{
    SpirvData = CompileGLSL(GLSLsource, ShaderKind, Label);
}

void RENDERER_CORE::ShaderData::FromSpirV(const std::vector<uint32_t>& Data)
{
    SpirvData = Data;
}

void RENDERER_CORE::ShaderData::FromSpirV(const char* FileName)
{
    auto ShaderCode = ReadFile(FileName);
    if (ShaderCode.empty())
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Empty SPIR-V file!");
        throw std::runtime_error("Empty SPIR-V file!");
    }
    std::vector<uint32_t> Temp(ShaderCode.size() / sizeof(uint32_t));
    memcpy(Temp.data(), ShaderCode.data(), ShaderCode.size());
    std::swap(SpirvData, Temp);
}

void RENDERER_CORE::ShaderData::WriteFileSpirv(const char* FileName)
{
    if (SpirvData.empty()) return;
    RENDERER_CORE::WriteFileSpirv(FileName,SpirvData);
}

size_t RENDERER_CORE::ShaderData::HashShaderData()
{
    size_t ResultingHash = 17;
    for (auto& Instruction : SpirvData)
    {
        ResultingHash = COMMON::CombineHash(ResultingHash, std::hash<uint32_t>()(Instruction));
    }
    return ResultingHash;
}
