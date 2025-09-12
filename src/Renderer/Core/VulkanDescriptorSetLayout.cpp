#include "VulkanDescriptorSetLayout.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

void RENDERER_CORE::DescriptorSetLayout::AppendLayoutBinding(VkDescriptorType DescriptorType, uint32_t DescriptorCount,uint32_t Binding,VkShaderStageFlags ShaderStage)
{
    VkDescriptorSetLayoutBinding NewLayoutBinding{};
    NewLayoutBinding.descriptorType = DescriptorType;
    NewLayoutBinding.binding = Binding;
    NewLayoutBinding.descriptorCount = DescriptorCount;
    NewLayoutBinding.stageFlags = ShaderStage;
    NewLayoutBinding.pImmutableSamplers = nullptr;

    this->Bindings.push_back(NewLayoutBinding);
}

void RENDERER_CORE::DescriptorSetLayout::CreateLayout(VkDevice& LogicalDevice,VkDescriptorSetLayoutCreateFlags Flags,void *Next)
{
    VkDescriptorSetLayoutCreateInfo LayoutCreateInfo{};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.bindingCount = static_cast<uint32_t>(Bindings.size());
    LayoutCreateInfo.pBindings = Bindings.data();
    LayoutCreateInfo.flags = Flags;
    LayoutCreateInfo.pNext = Next;

    if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutCreateInfo, nullptr, &this->Handle) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed creating descriptor layout [binding count(" + std::to_string(Bindings.size()) + ")]."));
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Created descriptor layout [binding count(" + std::to_string(Bindings.size()) + ")]."));
    Bindings.clear();
}

void RENDERER_CORE::DescriptorSetLayout::Destroy(VkDevice& LogicalDevice)
{
    if (Handle)
    {
        vkDestroyDescriptorSetLayout(LogicalDevice, this->Handle, nullptr);
        Handle = VK_NULL_HANDLE;
    }
}