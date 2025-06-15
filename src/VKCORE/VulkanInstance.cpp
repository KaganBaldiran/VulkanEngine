#include "VulkanInstance.hpp"
#include <iostream>
#include "VulkanSwapChain.hpp"

VkResult VKCORE::CreateDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto Func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (Func != nullptr)
    {
        return Func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VKCORE::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto Func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (Func != nullptr)
    {
        Func(instance, debugMessenger, pAllocator);
    }
}

void VKCORE::PopulateMessagerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& CreateInfo)
{
    CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    CreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    CreateInfo.pfnUserCallback = debugCallback;
}

void VKCORE::SetupDebugMessenger(VkInstance &Instance,const bool EnableValidationLayers,const std::vector<const char*>& ValidationLayers,VkDebugUtilsMessengerEXT& DestinationMessenger)
{
    if (!EnableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT CreateInfo{};
    VKCORE::PopulateMessagerCreateInfo(CreateInfo);

    if (VKCORE::CreateDebugUtilsMessengerEXT(Instance, &CreateInfo, nullptr, &DestinationMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to setup debug messenger!");
    }
}

bool VKCORE::CheckValidationLayerSupport(const std::vector<const char*>& ValidationLayers)
{
    uint32_t LayerCount;
    vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
    std::vector<VkLayerProperties> AvailableLayers(LayerCount);
    vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

    for (const char* LayerName : ValidationLayers)
    {
        bool layerFound = false;
        for (const auto& AvailableLayer : AvailableLayers)
        {
            if (strcmp(AvailableLayer.layerName, LayerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) return false;
    }
    return true;
}

VKCORE::VulkanResult VKCORE::CreateVulkanInstance(
    VulkanInstanceCreateInfo &InstanceCreateInfo,
    const std::vector<const char*>& ValidationLayers,
    bool EnableValidationLayers,
    VkInstance& OutInstance,
    VkDebugUtilsMessengerCreateInfoEXT* OutDebugMessengerCreateInfo)
{
    if (EnableValidationLayers && !VKCORE::CheckValidationLayerSupport(ValidationLayers))
    {
        return { VK_INCOMPLETE, "Requested validation layers aren't available!" };
    }

    VkApplicationInfo AppInfo{};
    AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pApplicationName = InstanceCreateInfo.ApplicationName.c_str();
    AppInfo.applicationVersion = InstanceCreateInfo.ApplicationVersion;
    AppInfo.pEngineName = InstanceCreateInfo.EngineName.c_str();
    AppInfo.engineVersion = InstanceCreateInfo.EngineVersion;
    AppInfo.apiVersion = InstanceCreateInfo.APIVersion;

    VkInstanceCreateInfo CreateInfo{};
    CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo = &AppInfo;

    uint32_t GLFWextensionsCount = 0;
    const char** GLFWextensionsString = glfwGetRequiredInstanceExtensions(&GLFWextensionsCount);

    std::vector<const char*> GLFWRequiredExtensions(GLFWextensionsCount);
    for (size_t i = 0; i < GLFWextensionsCount; i++)
    {
        GLFWRequiredExtensions[i] = GLFWextensionsString[i];
        std::cout << GLFWextensionsString[i] << std::endl;
    }

    if (EnableValidationLayers)
    {
        GLFWRequiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        GLFWRequiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    CreateInfo.enabledExtensionCount = static_cast<uint32_t>(GLFWRequiredExtensions.size());
    CreateInfo.ppEnabledExtensionNames = GLFWRequiredExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo{};
    if (EnableValidationLayers)
    {
        CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
        CreateInfo.ppEnabledLayerNames = ValidationLayers.data();

        VKCORE::PopulateMessagerCreateInfo(DebugCreateInfo);
        CreateInfo.pNext = &DebugCreateInfo;

        if (OutDebugMessengerCreateInfo != nullptr)
        {
            *OutDebugMessengerCreateInfo = DebugCreateInfo;
        }
    }
    else
    {
        CreateInfo.enabledLayerCount = 0;
        CreateInfo.pNext = nullptr;
    }

    uint32_t ExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

    std::vector<VkExtensionProperties> Extensions(ExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

    std::cout << "Available extensions: " << std::endl;

    unsigned int FoundExtensionsCount = 0;
    for (const auto& extension : Extensions)
    {
        for (const auto& GLFWextention : GLFWRequiredExtensions)
        {
            if (strcmp(GLFWextention, extension.extensionName) == 0)
            {
                FoundExtensionsCount++;
            }
        }
        std::cout << '\t' << extension.extensionName << std::endl;
    }

    if (FoundExtensionsCount != GLFWRequiredExtensions.size())
    {
        std::cout << "Some of the required extensions seem to be missing!" << std::endl;
    }

    if (vkCreateInstance(&CreateInfo, nullptr, &OutInstance) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE, "Failed to create an instance!" };
    }

    return VULKAN_SUCCESS;
}

VKCORE::VulkanResult VKCORE::SetupDebugMessenger(
    VkInstance &Instance,
    bool EnableValidationLayers,
    VkDebugUtilsMessengerEXT &DebugMessenger
)
{
    if (!EnableValidationLayers) return VULKAN_SUCCESS;
    VkDebugUtilsMessengerCreateInfoEXT CreateInfo{};
    VKCORE::PopulateMessagerCreateInfo(CreateInfo);

    if (VKCORE::CreateDebugUtilsMessengerEXT(Instance, &CreateInfo, nullptr, &DebugMessenger) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE ,"Failed to setup debug messenger!" };
    }
    return VULKAN_SUCCESS;
}

VKCORE::Instance::Instance(VulkanInstanceCreateInfo &CreateInfo)
{
    VKCORE::CreateVulkanInstance(CreateInfo, ValidationLayers, EnableValidationLayers, instance, nullptr);
    VKCORE::SetupDebugMessenger(instance, EnableValidationLayers, ValidationLayers, DebugMessenger);
}

void VKCORE::Instance::Destroy()
{
    DestroyDebugUtilsMessengerEXT(instance, DebugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);
}
