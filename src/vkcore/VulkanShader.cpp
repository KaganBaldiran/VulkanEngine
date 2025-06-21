#include "VulkanShader.hpp"
#include <fstream>

VkShaderModule VKCORE::CreateModule(const std::vector<char>& CodeSource,VkDevice& LogicalDevice)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo{};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = CodeSource.size();
    ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(CodeSource.data());

    VkShaderModule Module;
    if (vkCreateShaderModule(LogicalDevice, &ShaderModuleCreateInfo, nullptr, &Module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
    }
    return Module;
}

std::vector<char> VKCORE::ReadFile(const char* FileName)
{
    std::ifstream File(FileName, std::ios::ate | std::ios::binary);

    if (!File.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    size_t FileSize = static_cast<size_t>(File.tellg());
    std::vector<char> Buffer(FileSize);

    File.seekg(0);
    File.read(Buffer.data(), FileSize);
    File.close();
    return Buffer;
}

int VKCORE::CompileGLSL(const std::string& SourceFileName, const std::string& DestinationFileName)
{
    auto Command = std::string(".\\shaders\\compile.bat ") + SourceFileName + std::string(" ") + DestinationFileName;
    int Result = std::system(Command.c_str());
    if (Result != 0)
    {
        throw std::runtime_error("Failed to compile glsl into spir-v");
    }
}

VKCORE::ShaderModule::ShaderModule(const char* FileName, const char* SpirvFileName, bool CompileShaderIntoSpirv, VkDevice& LogicalDevice)
{
    Create(FileName, SpirvFileName, CompileShaderIntoSpirv, LogicalDevice);
}

void VKCORE::ShaderModule::Create(const char* FileName, const char* SpirvFileName, bool CompileShaderIntoSpirv, VkDevice& LogicalDevice)
{
    static unsigned int ShaderIterator = 0;
    std::string Name(FileName);
    auto FoundDot = Name.find_last_of('.');
    auto FoundSlash = Name.find_last_of('\\');
    std::string SpvName;
    if (FoundDot != std::string::npos && FoundSlash != std::string::npos) {
        SpvName = Name.substr(FoundSlash, FoundDot) + ".spv";
        Name = Name.substr(FoundSlash, Name.size() - 1);
    }

    if (CompileShaderIntoSpirv) CompileGLSL(FileName, SpirvFileName);
    auto ShaderCode = ReadFile(SpirvFileName);
    this->Module = CreateModule(ShaderCode, LogicalDevice);
    ShaderIterator++;
}

void VKCORE::ShaderModule::Destroy(VkDevice& LogicalDevice)
{
    vkDestroyShaderModule(LogicalDevice, Module, nullptr);
}
