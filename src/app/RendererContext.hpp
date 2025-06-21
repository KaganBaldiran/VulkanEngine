#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "../vkcore/VulkanUtils.hpp" 
#include <vector>

#include "../vkcore/VulkanCommandPool.hpp"
#include "../vkcore/VulkanCommandBuffer.hpp"
#include "../vkcore/VulkanDescriptorPool.hpp"
#include "../vkcore/VulkanDescriptorSetLayout.hpp"
#include "../vkcore/VulkanDescriptorSet.hpp"
#include "../vkcore/VulkanDevice.hpp"
#include "../vkcore/VulkanPipeline.hpp"
#include "../vkcore/VulkanImage.hpp"
#include "../vkcore/VulkanImageView.hpp"
#include "../vkcore/VulkanWindow.hpp"
#include "../vkcore/VulkanDevice.hpp"
#include "../vkcore/VulkanSwapChain.hpp"
#include "../vkcore/VulkanInstance.hpp"
#include "../vkcore/VulkanRender.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "../vkcore/VertexInputDescription.hpp"

const int MAX_FRAMES_IN_FLIGHT = 3;

namespace VKAPP
{
	class RendererContext
	{
	public:
        RendererContext(bool EnableValidationLayers);
        RendererContext() = default;
        void Create(bool EnableValidationLayers);
		void Destroy();

        void WaitDeviceIdle();

        VKCORE::Window Window;
        VKCORE::Instance Instance;
        VKCORE::Surface Surface;
        VKCORE::DeviceContext DeviceContext;
        VKCORE::CommandPool CommandPool;

        VKCORE::SwapChain SwapChain;
        VKCORE::QueueFamilyIndices QueueFamilyIndices;
        bool EnableValidationLayers;

        VKCORE::DescriptorSetLayout SceneDescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> SceneDescriptorSetLayouts;
	private:
	};
}
