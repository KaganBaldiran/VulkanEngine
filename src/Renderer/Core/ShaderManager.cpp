#include "ShaderManager.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"

RENDERER_CORE::ShaderModule* RENDERER_CORE::ShaderManager::AppendShaderModule(
    const char* ShaderLabel, 
    const char* GLSLSourceFileName, 
    const char* SpirvSourceFileName, 
    shaderc_shader_kind ShaderType, 
    VkDevice LogicalDevice
)
{
    RENDERER_CORE::ShaderModule NewModule{};
    NewModule.CompileOrLoadShaderModule(
        GLSLSourceFileName, 
        SpirvSourceFileName, 
        ShaderType, 
        ShaderLabel, 
        LogicalDevice
    );
    auto [Iterator,IsInserted] = ShaderModules.insert({ ShaderLabel,std::move(NewModule) });
    return &Iterator->second;
}

RENDERER_CORE::ShaderModule* RENDERER_CORE::ShaderManager::AppendShaderModule(const char* ShaderLabel,const RENDERER_CORE::ShaderModule& Module)
{
    auto [Iterator, IsInserted] = ShaderModules.insert({ ShaderLabel,Module });
    return &Iterator->second;
}

RENDERER_CORE::ShaderModule* RENDERER_CORE::ShaderManager::AppendShaderModule(const char* ShaderLabel,RENDERER_CORE::ShaderData& Data, VkDevice LogicalDevice)
{
    RENDERER_CORE::ShaderModule NewModule;
    NewModule.Create(Data, ShaderLabel, LogicalDevice);
    auto [Iterator, IsInserted] = ShaderModules.insert({ ShaderLabel,std::move(NewModule)});
    return &Iterator->second;
}

void RENDERER_CORE::ShaderManager::EraseShaderModule(std::string ShaderLabel, VkDevice LogicalDevice)
{
    auto Iterator = ShaderModules.find(ShaderLabel);
    if (Iterator == ShaderModules.end()) {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_WARNING, "Attempting to erase a shader module with an unrecognized label (" + ShaderLabel + ").");
        return;
    };
    Iterator->second.Destroy(LogicalDevice);
    ShaderModules.erase(Iterator);
}

RENDERER_CORE::ShaderModule* RENDERER_CORE::ShaderManager::GetShaderModule(std::string ShaderLabel)
{
    auto Iterator = ShaderModules.find(ShaderLabel);
    if (Iterator == ShaderModules.end()) return nullptr;
    return &Iterator->second;
}

void RENDERER_CORE::ShaderManager::Destroy(VkDevice LogicalDevice)
{
    for (auto& [Key,ShaderModule] : ShaderModules)
    {
        ShaderModule.Destroy(LogicalDevice);
    }
}
