#include "VulkanDevice.hpp"
#include <map>
#include <set>

VKCORE::VulkanResult VKCORE::CreateSurface(VkInstance& Instance,GLFWwindow* Window,VkSurfaceKHR& Surface)
{
    if (glfwCreateWindowSurface(Instance, Window, nullptr, &Surface) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE,"Failed to create a window surface!" };
    }
    return VULKAN_SUCCESS;
}

VKCORE::QueueFamilyIndices VKCORE::FindQueueFamilies(VkPhysicalDevice Device,VkSurfaceKHR &Surface)
{
    VKCORE::QueueFamilyIndices Indices;

    uint32_t QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilies.data());

    int i = 0;
    for (const auto& QueueFamily : QueueFamilies)
    {
        VkBool32 DoesSupportPresent = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, Surface, &DoesSupportPresent);
        if (QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            Indices.GraphicsFamily = i;
        }
        if (DoesSupportPresent)
        {
            Indices.PresentFamily = i;
        }

        if (Indices.isComplete())
        {
            break;
        }
        i++;
    }
    return Indices;
}

bool VKCORE::CheckDeviceExtensionSupport(VkPhysicalDevice Device, const std::vector<const char*> &DeviceExtensions)
{
    uint32_t ExtensionCount;
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, nullptr);

    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, AvailableExtensions.data());

    std::set<std::string> RequiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

    for (const auto& Extension : AvailableExtensions)
    {
        RequiredExtensions.erase(Extension.extensionName);
    }

    return RequiredExtensions.empty();
}

VKCORE::SwapChainSupportDetails VKCORE::QuerySwapChainSupport(VkPhysicalDevice Device,VkSurfaceKHR &Surface)
{
    VKCORE::SwapChainSupportDetails Details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Surface, &Details.Capabilities);

    uint32_t FormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, nullptr);

    if (FormatCount != 0)
    {
        Details.Formats.resize(FormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, Details.Formats.data());
    }

    uint32_t PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, nullptr);

    if (PresentModeCount != 0)
    {
        Details.PresentModes.resize(PresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, Details.PresentModes.data());
    }

    return Details;
}

int VKCORE::CheckDeviceSuitability(VkPhysicalDevice Device,VkSurfaceKHR& Surface,const std::vector<const char*> DeviceExtensions)
{
    VkPhysicalDeviceProperties DeviceProperties;
    vkGetPhysicalDeviceProperties(Device, &DeviceProperties);

    VkPhysicalDeviceFeatures DeviceFeatures;
    vkGetPhysicalDeviceFeatures(Device, &DeviceFeatures);
    if (!DeviceFeatures.geometryShader) return 0;

    int Score = 0;
    if (DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        Score += 1000;
    }

    Score += DeviceProperties.limits.maxImageDimension2D;
    VKCORE::QueueFamilyIndices indices = VKCORE::FindQueueFamilies(Device, Surface);

    bool IsExtensionsSupported = VKCORE::CheckDeviceExtensionSupport(Device, DeviceExtensions);

    Score *= static_cast<int>(indices.GraphicsFamily.has_value());
    Score *= static_cast<int>(IsExtensionsSupported);

    if (IsExtensionsSupported)
    {
        VKCORE::SwapChainSupportDetails SwapChainSupport = VKCORE::QuerySwapChainSupport(Device,Surface);
        Score *= static_cast<int>(!SwapChainSupport.Formats.empty() && !SwapChainSupport.PresentModes.empty());
    }

    return Score;
}

VKCORE::VulkanResult VKCORE::PickPhysicalDevice(VulkanDeviceCreateInfo& CreateInfo,VkInstance &Instance,VkPhysicalDevice& DestinationDevice,VkSurfaceKHR &Surface)
{
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);

    if (DeviceCount == 0)
    {
        return { VK_INCOMPLETE,"Failed to detect GPUs with Vulkan support!" };
    }

    std::vector<VkPhysicalDevice> PhyDevices(DeviceCount);
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, PhyDevices.data());

    std::multimap<int, VkPhysicalDevice>  Candidates;

    for (auto& Device : PhyDevices)
    {
        int Score;
        if (CreateInfo.PhysicalDeviceScoreEvalOperation) Score = CreateInfo.PhysicalDeviceScoreEvalOperation(Device, Surface);
        else Score = VKCORE::CheckDeviceSuitability(Device, Surface,CreateInfo.DeviceExtensionsToEnable);
        Candidates.insert({ Score,Device });
    }

    if (Candidates.rbegin()->first > 0)
    {
        DestinationDevice = Candidates.rbegin()->second;
    }
    else
    {
        return { VK_INCOMPLETE,"Failed to detect a suitable physical device!" };
    }

    VkPhysicalDeviceProperties DeviceProperties;
    vkGetPhysicalDeviceProperties(DestinationDevice, &DeviceProperties);
    std::cout << "Physical Device: " << DeviceProperties.deviceName << std::endl;
    return VULKAN_SUCCESS;
}

VKCORE::VulkanResult VKCORE::CreateLogicalDevice(
    VulkanDeviceCreateInfo& CreateInfo,
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    VkDevice& LogicalDevice,
    VkQueue& GraphicsQueue,
    VkQueue& PresentQueue)
{
    VKCORE::QueueFamilyIndices indices = VKCORE::FindQueueFamilies(PhysicalDevice, Surface);

    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    std::set<uint32_t> UniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

    QueueCreateInfos.reserve(UniqueQueueFamilies.size());

    for (uint32_t QueueFamily : UniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo QueueCreateInfo{};
        QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCreateInfo.queueFamilyIndex = QueueFamily;
        QueueCreateInfo.queueCount = 1;
        QueueCreateInfo.pQueuePriorities = &CreateInfo.QueuePriority;
        QueueCreateInfos.push_back(QueueCreateInfo);
    }

    VkPhysicalDeviceFeatures DeviceFeatures{};
    DeviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo DeviceCreateInfo{};
    DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
    DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
    DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;
    DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(CreateInfo.DeviceExtensionsToEnable.size());
    DeviceCreateInfo.ppEnabledExtensionNames = CreateInfo.DeviceExtensionsToEnable.data();

    VkPhysicalDeviceDynamicRenderingFeaturesKHR DynamicRenderingFeatures{};
    DynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    DynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    DeviceCreateInfo.pNext = &DynamicRenderingFeatures;

    if (vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &LogicalDevice) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE, "Failed to create the logical device!" };
    }

    vkGetDeviceQueue(LogicalDevice, indices.GraphicsFamily.value(), 0, &GraphicsQueue);
    vkGetDeviceQueue(LogicalDevice, indices.PresentFamily.value(), 0, &PresentQueue);

    return VULKAN_SUCCESS;
}


VKCORE::Surface::Surface(VkInstance &Instance,GLFWwindow *Window)
{
    Create(Instance, Window);
};

void VKCORE::Surface::Create(VkInstance& Instance, GLFWwindow* Window)
{
    CreateSurface(Instance, Window, surface);
}

void VKCORE::Surface::Destroy(VkInstance &Instance)
{
    vkDestroySurfaceKHR(Instance, surface, nullptr);
}

VKCORE::DeviceContext::DeviceContext(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance)
{
    Create(CreateInfo, Surface, Instance);
}

void VKCORE::DeviceContext::Create(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance)
{
    PickPhysicalDevice(CreateInfo, Instance, physicalDevice, Surface);
    CreateLogicalDevice(CreateInfo, physicalDevice, Surface, logicalDevice, GraphicsQueue, PresentQueue);
}

void VKCORE::DeviceContext::Destroy()
{
    vkDestroyDevice(logicalDevice, nullptr);
}
