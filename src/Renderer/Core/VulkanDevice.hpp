#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include <iostream>
#include <string>
#include <optional>
#include "VulkanUtils.hpp"
#include <functional>

namespace VKCORE
{
    struct VulkanDeviceCreateInfo
    {
        std::function<int(VkPhysicalDevice&, VkSurfaceKHR&)> PhysicalDeviceScoreEvalOperation;
        std::vector<const char*> DeviceExtensionsToEnable;
        float QueuePriority = 1.0f;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> ComputeFamily;
        std::optional<uint32_t> PresentFamily;

        bool HasGraphics() { return GraphicsFamily.has_value(); };
        bool HasCompute() { return ComputeFamily.has_value(); };
        bool HasPresent() { return PresentFamily.has_value(); };

        bool isComplete() {
            return GraphicsFamily.has_value() && PresentFamily.has_value() && ComputeFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    VulkanResult CreateSurface(VkInstance& Instance, GLFWwindow* Window, VkSurfaceKHR& Surface);
  
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device, VkSurfaceKHR& Surface);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice Device, const std::vector<const char*>& DeviceExtensions);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR& Surface);
    int CheckDeviceSuitability(VkPhysicalDevice Device, VkSurfaceKHR& Surface, const std::vector<const char*> DeviceExtensions);
    VulkanResult PickPhysicalDevice(VulkanDeviceCreateInfo& CreateInfo, VkInstance& Instance, VkPhysicalDevice& DestinationDevice, VkSurfaceKHR& Surface);

    VulkanResult CreateLogicalDevice(
        VulkanDeviceCreateInfo& CreateInfo,
        VkPhysicalDevice PhysicalDevice,
        VkSurfaceKHR Surface,
        VkDevice& LogicalDevice,
        VkQueue& GraphicsQueue,
        VkQueue& PresentQueue,
        VkQueue& ComputeQueue
    );


    class Surface
    {
    public:
        Surface(VkInstance& Instance, GLFWwindow* Window);
        Surface() = default;
        void Create(VkInstance& Instance, GLFWwindow* Window);
        void Destroy(VkInstance &Instance);

        VkSurfaceKHR surface;
    private:
    };

    class DeviceContext
    {
    public:
        DeviceContext(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance);
        DeviceContext() = default;
        void Create(VulkanDeviceCreateInfo& CreateInfo, VkSurfaceKHR& Surface, VkInstance& Instance);
        void Destroy();

        VkPhysicalDeviceFeatures DeviceFeatures;
        VkPhysicalDeviceProperties DeviceProperties;
        VkDevice logicalDevice;
        VkPhysicalDevice physicalDevice;
        VkQueue PresentQueue;
        VkQueue GraphicsQueue;
        VkQueue ComputeQueue;
    private:
    };
}