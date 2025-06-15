#pragma once
#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>
#include "VulkanUtils.hpp"

namespace VKCORE
{

    struct VulkanInstanceCreateInfo
    {
        uint32_t ApplicationVersion;
        std::string ApplicationName;
        uint32_t EngineVersion;
        std::string EngineName;
        uint32_t APIVersion;

        bool EnableValidationLayers;
        std::vector<const char*> ValidationLayersToEnable;
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator);

    void PopulateMessagerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& CreateInfo);
    void SetupDebugMessenger(VkInstance& Instance, const bool EnableValidationLayers, const std::vector<const char*>& ValidationLayers, VkDebugUtilsMessengerEXT& DestinationMessenger);
	bool CheckValidationLayerSupport(const std::vector<const char*>& ValidationLayers);

    VulkanResult CreateVulkanInstance(
        VulkanInstanceCreateInfo &InstanceCreateInfo,
        const std::vector<const char*>& ValidationLayers,
        bool EnableValidationLayers,
        VkInstance& OutInstance,
        VkDebugUtilsMessengerCreateInfoEXT* OutDebugMessengerCreateInfo);

    VulkanResult SetupDebugMessenger(
        VkInstance& Instance,
        bool EnableValidationLayers,
        VkDebugUtilsMessengerEXT& DebugMessenger
    );

    class Instance
    {
    public:
        Instance(VulkanInstanceCreateInfo& CreateInfo);
        void Destroy();
        VkInstance instance;
        bool EnableValidationLayers;
        std::vector<const char*> ValidationLayers;
        VkDebugUtilsMessengerEXT DebugMessenger;
    private:
    };
}