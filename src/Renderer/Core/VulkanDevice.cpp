#include "VulkanDevice.hpp"
#include <map>
#include <set>

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"
#include <vulkan/vk_enum_string_helper.h>

RENDERER_CORE::VulkanResult RENDERER_CORE::CreateSurface(VkInstance& Instance,GLFWwindow* Window,VkSurfaceKHR& Surface)
{
    if (glfwCreateWindowSurface(Instance, Window, nullptr, &Surface) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE,"Failed to create a window surface!" };
    }
    return VULKAN_SUCCESS;
}

RENDERER_CORE::QueueFamilyIndices RENDERER_CORE::FindQueueFamilies(VkPhysicalDevice Device,VkSurfaceKHR &Surface)
{
    RENDERER_CORE::QueueFamilyIndices Indices;

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
        if ((QueueFamily.queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))
        {
            Indices.GraphicsComputeFamily = i;
        }
        if ((QueueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            Indices.ComputeFamily = i;
        }
        if ((QueueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(QueueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            Indices.TransferFamily = i;
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
/*
bool RENDERER_CORE::CheckDeviceExtensionSupport(VkPhysicalDevice Device, const std::vector<const char*> &DeviceExtensions)
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
*/

bool RENDERER_CORE::CheckDeviceExtensionSupport(VkPhysicalDevice Device, const std::vector<DeviceFeature>& DeviceExtensions)
{
    uint32_t ExtensionCount;
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, nullptr);

    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, AvailableExtensions.data());

    for (const auto& Extension : DeviceExtensions)
    {
        if (Extension.ExtensionName.empty()) continue;
        bool Found = false;
        for (const auto& AvailableExtension : AvailableExtensions)
        {
            if (strcmp(AvailableExtension.extensionName, Extension.ExtensionName.c_str()))
            {
                Found = true;
                break;
            }
        }
        if (!Found && Extension.Required) return false;
    }
    return true;
}

bool RENDERER_CORE::EvaluateDeviceExtensions(VkPhysicalDevice Device, std::vector<DeviceFeature>& DeviceExtensions)
{
   

    return false;
}

RENDERER_CORE::SwapChainSupportDetails RENDERER_CORE::QuerySwapChainSupport(VkPhysicalDevice Device,VkSurfaceKHR &Surface)
{
    RENDERER_CORE::SwapChainSupportDetails Details;

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

int RENDERER_CORE::CheckDeviceSuitability(VkPhysicalDevice Device,VkSurfaceKHR& Surface,const std::vector<DeviceFeature> DeviceExtensions)
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
    RENDERER_CORE::QueueFamilyIndices indices = RENDERER_CORE::FindQueueFamilies(Device, Surface);

    bool IsExtensionsSupported = RENDERER_CORE::CheckDeviceExtensionSupport(Device, DeviceExtensions);
    Score *= static_cast<int>(IsExtensionsSupported);
    Score *= static_cast<int>(indices.GraphicsFamily.has_value());

    if (IsExtensionsSupported)
    {
        RENDERER_CORE::SwapChainSupportDetails SwapChainSupport = RENDERER_CORE::QuerySwapChainSupport(Device,Surface);
        Score *= static_cast<int>(!SwapChainSupport.Formats.empty() && !SwapChainSupport.PresentModes.empty());
    }

    return Score;
}

RENDERER_CORE::VulkanResult RENDERER_CORE::PickPhysicalDevice(VulkanDeviceCreateInfo& CreateInfo,VkInstance &Instance,VkPhysicalDevice& DestinationDevice,VkSurfaceKHR &Surface)
{
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);

    if (DeviceCount == 0)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Failed to detect GPUs with Vulkan support!");
        return { VK_INCOMPLETE,"Failed to detect GPUs with Vulkan support!" };
    }

    std::vector<VkPhysicalDevice> PhyDevices(DeviceCount);
    vkEnumeratePhysicalDevices(Instance, &DeviceCount, PhyDevices.data());

    std::multimap<int, VkPhysicalDevice>  Candidates;

    for (auto& Device : PhyDevices)
    {
        int Score;
        if (CreateInfo.PhysicalDeviceScoreEvalOperation) Score = CreateInfo.PhysicalDeviceScoreEvalOperation(Device, Surface);
        else Score = RENDERER_CORE::CheckDeviceSuitability(Device, Surface,CreateInfo.RequestedDeviceFeatureNodes);
        Candidates.insert({ Score,Device });
    }

    if (Candidates.rbegin()->first > 0)
    {
        DestinationDevice = Candidates.rbegin()->second;
    }
    else
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Failed to detect a suitable physical device!");
        return { VK_INCOMPLETE,"Failed to detect a suitable physical device!" };
    }

    return VULKAN_SUCCESS;
}

RENDERER_CORE::VulkanResult RENDERER_CORE::CreateLogicalDevice(
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    VkDevice& LogicalDevice,
    VkQueue& GraphicsQueue,
    VkQueue& PresentQueue,
    VkQueue& ComputeQueue,
    VkQueue& TransferQueue,
    VkQueue& GraphicsComputeQueue,
    float QueuePriority,
    VkBaseOutStructure* FeaturesToEnable,
    std::vector<const char*> ExtensionsToEnable,
    bool EnableValidationLayers,
    std::vector<const char*> ValidationLayersToEnable
)
{
    RENDERER_CORE::QueueFamilyIndices indices = RENDERER_CORE::FindQueueFamilies(PhysicalDevice, Surface);

    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    std::set<uint32_t> UniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value(), };
    if (indices.HasCompute()) UniqueQueueFamilies.insert(indices.ComputeFamily.value());
    if (indices.HasComputeGraphics()) UniqueQueueFamilies.insert(indices.GraphicsComputeFamily.value());
    if (indices.HasTransfer()) UniqueQueueFamilies.insert(indices.TransferFamily.value());

    QueueCreateInfos.reserve(UniqueQueueFamilies.size());

    for (uint32_t QueueFamily : UniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo QueueCreateInfo{};
        QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCreateInfo.queueFamilyIndex = QueueFamily;
        QueueCreateInfo.queueCount = 1;
        QueueCreateInfo.pQueuePriorities = &QueuePriority;
        QueueCreateInfos.push_back(QueueCreateInfo);
    }

    VkPhysicalDeviceFeatures DeviceFeatures{};
    DeviceFeatures.samplerAnisotropy = VK_TRUE;
    DeviceFeatures.multiDrawIndirect = VK_TRUE;

    VkDeviceCreateInfo DeviceCreateInfo{};
    DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
    DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
    DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;
    DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(ExtensionsToEnable.size());
    DeviceCreateInfo.ppEnabledExtensionNames = ExtensionsToEnable.data();
    DeviceCreateInfo.pNext = FeaturesToEnable;

    if (EnableValidationLayers)
    {
        DeviceCreateInfo.ppEnabledLayerNames = ValidationLayersToEnable.data();
        DeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayersToEnable.size());
    }
    else
    {
        DeviceCreateInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &LogicalDevice) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Failed to create device.");
        return { VK_INCOMPLETE, "Failed to create the logical device!" };
    }

    if (indices.HasGraphics())
    {
        vkGetDeviceQueue(LogicalDevice, indices.GraphicsFamily.value(), 0, &GraphicsQueue);
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected graphics queue [" + std::to_string(reinterpret_cast<uintptr_t>(GraphicsQueue)) + "].");
    }
    if (indices.HasPresent())
    {
        vkGetDeviceQueue(LogicalDevice, indices.PresentFamily.value(), 0, &PresentQueue);
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected present queue [" + std::to_string(reinterpret_cast<uintptr_t>(PresentQueue)) + "].");
    }
    if (indices.HasCompute())
    {
        vkGetDeviceQueue(LogicalDevice, indices.ComputeFamily.value(), 0, &ComputeQueue);
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected compute queue [" + std::to_string(reinterpret_cast<uintptr_t>(ComputeQueue)) + "].");
    }
    if (indices.HasTransfer())
    {
        vkGetDeviceQueue(LogicalDevice, indices.TransferFamily.value(), 0, &TransferQueue);
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected transfer queue [" + std::to_string(reinterpret_cast<uintptr_t>(TransferQueue)) + "].");
    }
    if (indices.HasComputeGraphics())
    {
        vkGetDeviceQueue(LogicalDevice, indices.GraphicsComputeFamily.value(), 0, &GraphicsComputeQueue);
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected graphics-compute queue [" + std::to_string(reinterpret_cast<uintptr_t>(GraphicsComputeQueue)) + "].");
    }

    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Created device [" + std::to_string(reinterpret_cast<uintptr_t>(LogicalDevice)) + "].");
    return VULKAN_SUCCESS;
}


RENDERER_CORE::Surface::Surface(VkInstance &Instance,GLFWwindow *Window)
{
    Create(Instance, Window);
};

void RENDERER_CORE::Surface::Create(VkInstance& Instance, GLFWwindow* Window)
{
    CreateSurface(Instance, Window, Handle);
}

void RENDERER_CORE::Surface::Destroy(VkInstance &Instance)
{
    vkDestroySurfaceKHR(Instance, Handle, nullptr);
}

RENDERER_CORE::DeviceContext::DeviceContext(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance)
{
    Create(CreateInfo, Surface, Instance);
}

void RENDERER_CORE::DeviceContext::Create(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance)
{
    VULKAN_ASSERT_RESULT(PickPhysicalDevice(CreateInfo, Instance, PhysicalDevice, Surface));

    vkGetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);

    DeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    AccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    RayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    RayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    uint32_t ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);

    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

    VkBaseOutStructure* FeatureLine = reinterpret_cast<VkBaseOutStructure*>(&AccelerationStructureFeatures);
    std::vector<const char*> SupportedExtensionNames;
    SupportedExtensionNames.reserve(CreateInfo.RequestedDeviceFeatureNodes.size());
    
    std::vector<RENDERER_CORE::DeviceFeature*> PassedInitialCheck;
    PassedInitialCheck.reserve(CreateInfo.RequestedDeviceFeatureNodes.size());
    for (auto& Extension : CreateInfo.RequestedDeviceFeatureNodes)
    {
        bool IsSupported = false;
        if (!Extension.ExtensionName.empty())
        {
            for (const auto& AvailableExtension : AvailableExtensions)
            {
                if (strcmp(AvailableExtension.extensionName, Extension.ExtensionName.c_str()) == 0)
                {
                    IsSupported = true;
                    break;
                }
            }
            if (!IsSupported) continue;
            SupportedExtensionNames.push_back(Extension.ExtensionName.c_str());
        }
        if(Extension.FeatureStructure)
        {
            FeatureLine->pNext = reinterpret_cast<VkBaseOutStructure*>(Extension.FeatureStructure);
            FeatureLine = FeatureLine->pNext;
            FeatureLine->pNext = nullptr;

            PassedInitialCheck.push_back(&Extension);
        }
    }

    RayQueryFeatures.pNext = &AccelerationStructureFeatures;
    RayTracingPipelineFeatures.pNext = &RayQueryFeatures;
    DeviceFeatures2.pNext = &RayTracingPipelineFeatures;
    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &DeviceFeatures2);

    FeatureLine = reinterpret_cast<VkBaseOutStructure*>(&AccelerationStructureFeatures);
    FeatureLine->pNext = nullptr;
    for (auto& FeatureNode : PassedInitialCheck)
    {
        if (FeatureNode->CheckAvailability && FeatureNode->CheckAvailability())
        {
            FeatureLine->pNext = reinterpret_cast<VkBaseOutStructure*>(FeatureNode->FeatureStructure);
            FeatureLine = FeatureLine->pNext;
            FeatureLine->pNext = nullptr;
        }
        else if (FeatureNode->Required)
        {
            const char* FeatureName = string_VkStructureType(reinterpret_cast<VkBaseOutStructure*>(FeatureNode->FeatureStructure)->sType);
            LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_VERBOSE, "Physical Device " + std::string(DeviceProperties.deviceName) +
                " doesn't support a required feature (" + FeatureName + ")!");
            LOG_CONSOLE(COMMON::LOG_SEVERITY_VERBOSE,"Physical Device " + std::string(DeviceProperties.deviceName) +
                                    " doesn't support a required feature (" + FeatureName + ")! Exiting...")
            exit(-1);
        }
    }

    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, "Detected a suitable physical device [name(" + std::string(DeviceProperties.deviceName) +
        "), device type(" + string_VkPhysicalDeviceType(DeviceProperties.deviceType) + ")].");

    VULKAN_ASSERT_RESULT(CreateLogicalDevice(
        PhysicalDevice, 
        Surface, 
        LogicalDevice, 
        GraphicsQueue, 
        PresentQueue,
        ComputeQueue,
        TransferQueue,
        GraphicsComputeQueue,
        CreateInfo.QueuePriority,
        reinterpret_cast<VkBaseOutStructure*>(&AccelerationStructureFeatures),
        SupportedExtensionNames,
        CreateInfo.EnableValidationLayers,
        CreateInfo.ValidationLayersToEnable
    ));
}

void RENDERER_CORE::DeviceContext::Destroy()
{
    vkDestroyDevice(LogicalDevice, nullptr);
}

VkQueue RENDERER_CORE::DeviceContext::GetQueue(QueueType Type)
{
    switch (Type)
    {
        case QUEUE_TYPE_PRESENT:
        {
            return PresentQueue;
            break;
        }
        case QUEUE_TYPE_GRAPHICS:
        {
            return GraphicsQueue;
            break;
        }
        case QUEUE_TYPE_TRANSFER:
        {
            return TransferQueue;
            break;
        }
        case QUEUE_TYPE_COMPUTE:
        {
            return ComputeQueue;
            break;
        }
        case QUEUE_TYPE_GRAPHICS_COMPUTE:
        {
            return GraphicsComputeQueue;
            break;
        }
        default:
            return VK_NULL_HANDLE;
            break;
    }
}
