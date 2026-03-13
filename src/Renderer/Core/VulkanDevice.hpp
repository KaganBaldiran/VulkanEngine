#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include <iostream>
#include <string>
#include <optional>
#include "VulkanUtils.hpp"
#include <functional>

namespace RENDERER_CORE
{
    struct DeviceFeature
    {
        //Vulkan extention name
        std::string ExtensionName;
        //Feature structure thats passed through 
        void* FeatureStructure = nullptr;
        //Flag that makes the device creation dependent on this feature and extension
        bool Required = false;
        //Condition to check whether the desired feature in the feature struct is set.
        //Makes the given feature "Required"
        std::function<bool()> CheckAvailability;
    };

    struct VulkanDeviceCreateInfo
    {
        std::function<int(VkPhysicalDevice&, VkSurfaceKHR&)> PhysicalDeviceScoreEvalOperation;
        std::vector<DeviceFeature> RequestedDeviceFeatureNodes;
        //std::vector<const char*> DeviceExtensionsToEnable;
        float QueuePriority = 1.0f;
        void* FeaturesToEnable = nullptr;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> GraphicsComputeFamily;
        std::optional<uint32_t> ComputeFamily;
        std::optional<uint32_t> PresentFamily;
        std::optional<uint32_t> TransferFamily;

        bool HasGraphics() { return GraphicsFamily.has_value(); };
        bool HasCompute() { return ComputeFamily.has_value(); };
        bool HasPresent() { return PresentFamily.has_value(); };
        bool HasComputeGraphics() { return GraphicsComputeFamily.has_value(); };
        bool HasTransfer() { return TransferFamily.has_value(); };

        bool isComplete() {
            return GraphicsComputeFamily.has_value() && GraphicsFamily.has_value() && PresentFamily.has_value() && ComputeFamily.has_value();
        }
    };
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    VulkanResult CreateSurface(VkInstance& Instance, GLFWwindow* Window, VkSurfaceKHR& Surface);
  
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device, VkSurfaceKHR& Surface);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice Device, const std::vector<DeviceFeature>& DeviceExtensions);
    bool EvaluateDeviceExtensions(VkPhysicalDevice Device, std::vector<DeviceFeature>& DeviceExtensions);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR& Surface);
    int CheckDeviceSuitability(VkPhysicalDevice Device, VkSurfaceKHR& Surface, const std::vector<DeviceFeature> DeviceExtensions);
    VulkanResult PickPhysicalDevice(VulkanDeviceCreateInfo& CreateInfo, VkInstance& Instance, VkPhysicalDevice& DestinationDevice, VkSurfaceKHR& Surface);

    VulkanResult CreateLogicalDevice(
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
        std::vector<const char*> ExtensionsToEnable
    );

    class Surface
    {
    public:
        Surface(VkInstance& Instance, GLFWwindow* Window);
        Surface() = default;
        void Create(VkInstance& Instance, GLFWwindow* Window);
        void Destroy(VkInstance &Instance);

        VkSurfaceKHR Handle;
    private:
    };

    enum QueueType
    {
        QUEUE_TYPE_PRESENT = 0,
        QUEUE_TYPE_GRAPHICS = 1,
        QUEUE_TYPE_TRANSFER = 2,
        QUEUE_TYPE_COMPUTE = 3,
        QUEUE_TYPE_GRAPHICS_COMPUTE = 4,
        QUEUE_TYPE_SIZE
    };

    class DeviceContext
    {
    public:
        DeviceContext(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance);
        DeviceContext() = default;
        void Create(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance);
        void Destroy();

        VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures;
        VkPhysicalDeviceRayQueryFeaturesKHR RayQueryFeatures;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR RayTracingPipelineFeatures;
        VkPhysicalDeviceFeatures2 DeviceFeatures2;
        VkPhysicalDeviceProperties DeviceProperties;

        VkDevice LogicalDevice;
        VkPhysicalDevice PhysicalDevice;

        VkQueue GetQueue(QueueType Type);

        VkQueue PresentQueue = VK_NULL_HANDLE;
        VkQueue GraphicsQueue = VK_NULL_HANDLE;
        VkQueue TransferQueue = VK_NULL_HANDLE;
        VkQueue ComputeQueue = VK_NULL_HANDLE;
        VkQueue GraphicsComputeQueue = VK_NULL_HANDLE;
    private:
    };
}