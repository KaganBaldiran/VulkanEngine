#include "VulkanDescriptorSetLayout.hpp"

void VKCORE::DescriptorSetLayout::AppendLayoutBinding(VkDescriptorType DescriptorType, uint32_t DescriptorCount,uint32_t Binding,VkShaderStageFlags ShaderStage)
{
    VkDescriptorSetLayoutBinding NewLayoutBinding{};
    NewLayoutBinding.descriptorType = DescriptorType;
    NewLayoutBinding.binding = Binding;
    NewLayoutBinding.descriptorCount = DescriptorCount;
    NewLayoutBinding.stageFlags = ShaderStage;
    NewLayoutBinding.pImmutableSamplers = nullptr;

    this->Bindings.push_back(NewLayoutBinding);
}

void VKCORE::DescriptorSetLayout::CreateLayout(VkDevice& LogicalDevice)
{
    VkDescriptorSetLayoutCreateInfo LayoutCreateInfo{};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.bindingCount = static_cast<uint32_t>(Bindings.size());
    LayoutCreateInfo.pBindings = Bindings.data();

    if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutCreateInfo, nullptr, &this->descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void VKCORE::DescriptorSetLayout::Destroy(VkDevice& LogicalDevice)
{
    vkDestroyDescriptorSetLayout(LogicalDevice, this->descriptorSetLayout, nullptr);
}