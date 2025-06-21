#include "RendererContext.hpp"

VKAPP::RendererContext::RendererContext(bool EnableValidationLayers)
{
    Create(EnableValidationLayers);
}

void VKAPP::RendererContext::Create(bool EnableValidationLayers)
{
    VKCORE::VulkanWindowCreateInfo WindowCreateInfo{};
    WindowCreateInfo.WindowInitialHeight = 800;
    WindowCreateInfo.WindowInitialWidth = 1000;
    WindowCreateInfo.WindowsName = "Hello World";
    Window.Create(WindowCreateInfo);

    VKCORE::VulkanInstanceCreateInfo InstanceCreateInfo{};
    InstanceCreateInfo.APIVersion = VK_API_VERSION_1_3;
    InstanceCreateInfo.ApplicationName = "Application";
    InstanceCreateInfo.ApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineName = "No Engine";
    InstanceCreateInfo.EnableValidationLayers = EnableValidationLayers;
    InstanceCreateInfo.ValidationLayersToEnable = { "VK_LAYER_KHRONOS_validation" };
    Instance.Create(InstanceCreateInfo);

    Surface.Create(Instance.instance, Window.window);

    VKCORE::VulkanDeviceCreateInfo DeviceCreateInfo{};
    DeviceCreateInfo.DeviceExtensionsToEnable = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
    DeviceCreateInfo.QueuePriority = 1.0f;
    DeviceContext.Create(DeviceCreateInfo, Surface.surface, Instance.instance);

    SwapChain.Create(DeviceContext.physicalDevice, DeviceContext.logicalDevice, Surface.surface, Window.window);
    QueueFamilyIndices = VKCORE::FindQueueFamilies(DeviceContext.physicalDevice, Surface.surface);

    CommandPool.Create(QueueFamilyIndices.GraphicsFamily.value(), DeviceContext.logicalDevice);

    //Layout needed for the scene descriptor sets
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.CreateLayout(DeviceContext.logicalDevice);
    SceneDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, SceneDescriptorSetLayout.descriptorSetLayout);
}

void VKAPP::RendererContext::Destroy()
{
    SceneDescriptorSetLayout.Destroy(DeviceContext.logicalDevice);
    CommandPool.Destroy(DeviceContext.logicalDevice);
    Surface.Destroy(Instance.instance);
    SwapChain.Destroy(DeviceContext.logicalDevice);
    DeviceContext.Destroy();
    Instance.Destroy();
    Window.Destroy();
}
void VKAPP::RendererContext::WaitDeviceIdle()
{
    vkDeviceWaitIdle(DeviceContext.logicalDevice);
};
