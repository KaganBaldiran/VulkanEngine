#include "VulkanSwapChain.hpp"
#include "VulkanDevice.hpp"
#include <algorithm>
#include "VulkanImageView.hpp"

VkSurfaceFormatKHR VKCORE::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats)
{
    for (const auto& AvailableFormat : AvailableFormats)
    {
        if (AvailableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && AvailableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return AvailableFormat;
        }
    }

    return AvailableFormats[0];
}

VkPresentModeKHR VKCORE::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes)
{
    for (const auto& AvailablePresentMode : AvailablePresentModes)
    {
        if (AvailablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return AvailablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VKCORE::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities,GLFWwindow* Window)
{
    if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return Capabilities.currentExtent;
    }
    else
    {
        int Width, Height;
        glfwGetFramebufferSize(Window, &Width, &Height);

        VkExtent2D ActualExtent = {
            static_cast<uint32_t>(Width),
            static_cast<uint32_t>(Height)
        };

        ActualExtent.width = std::clamp(ActualExtent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        ActualExtent.height = std::clamp(ActualExtent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);

        return ActualExtent;
    }
}

VKCORE::VulkanResult VKCORE::CreateSwapChain(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkSurfaceKHR surface,
    GLFWwindow* window,
    VkSwapchainKHR& swapChain,
    std::vector<VkImage>& swapChainImages,
    VkSurfaceFormatKHR& surfaceFormat,
    VkPresentModeKHR& presentMode,
    VkExtent2D& extent
)
{
    VKCORE::SwapChainSupportDetails swapChainSupport = VKCORE::QuerySwapChainSupport(physicalDevice, surface);

    surfaceFormat = VKCORE::ChooseSwapSurfaceFormat(swapChainSupport.Formats);
    presentMode = VKCORE::ChooseSwapPresentMode(swapChainSupport.PresentModes);
    extent = VKCORE::ChooseSwapExtent(swapChainSupport.Capabilities, window);

    uint32_t swapChainImageCount = swapChainSupport.Capabilities.minImageCount + 1;
    if (swapChainSupport.Capabilities.maxImageCount > 0 && swapChainImageCount > swapChainSupport.Capabilities.maxImageCount)
    {
        swapChainImageCount = swapChainSupport.Capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = swapChainImageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VKCORE::QueueFamilyIndices indices = VKCORE::FindQueueFamilies(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

    if (indices.GraphicsFamily != indices.PresentFamily)
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    swapChainCreateInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        return { VK_INCOMPLETE, "Failed to create swap chain!" };
    }

    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, nullptr);
    swapChainImages.resize(swapChainImageCount);
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapChainImageCount, swapChainImages.data());
    return VULKAN_SUCCESS;
}

VKCORE::SwapChain::SwapChain(
    VkPhysicalDevice &PhysicalDevice,
    VkDevice &LogicalDevice,
    VkSurfaceKHR &Surface,
    GLFWwindow* Window
)
{
    Create(PhysicalDevice, LogicalDevice, Surface, Window);
}

void VKCORE::SwapChain::Destroy(VkDevice& LogicalDevice)
{
    for (size_t i = 0; i < SwapChainImagesViews.size(); i++)
    {
        vkDestroyImageView(LogicalDevice,SwapChainImagesViews[i], nullptr);
    }

    vkDestroySwapchainKHR(LogicalDevice, swapChain, nullptr);
    swapChain = VK_NULL_HANDLE;
}

void VKCORE::SwapChain::Create(
    VkPhysicalDevice& PhysicalDevice,
    VkDevice& LogicalDevice,
    VkSurfaceKHR& Surface,
    GLFWwindow* Window
)
{
    if (swapChain) return;
    CreateSwapChain(PhysicalDevice, LogicalDevice, Surface, Window, swapChain, SwapChainImages, SurfaceFormat, PresentMode, Extent);
    CreateSwapChainImageViews(SwapChainImages, SwapChainImagesViews, SurfaceFormat.format, LogicalDevice);
}
