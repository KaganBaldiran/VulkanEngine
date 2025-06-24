#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vkcore/VulkanImage.hpp"
#include <array>

namespace VKAPP
{
	//Forward Decleration
	class RendererContext;
}

namespace VKSCENE
{
	class Cubemap
	{
	public:
		Cubemap(VKAPP::RendererContext& RendererContext,uint32_t Width, uint32_t Height);
		Cubemap() = default;
		void Create(VKAPP::RendererContext& RendererContext,uint32_t Width, uint32_t Height);
		
		void Destroy(VKAPP::RendererContext& RendererContext);

		glm::ivec2 Size;
		VkImage Image = VK_NULL_HANDLE;
		VkDeviceMemory ImageMemory = VK_NULL_HANDLE;
		VkSampler Sampler = VK_NULL_HANDLE;
		std::array<VkImageView, 6> RenderImageViews;
		VkImageView SampleImageView;
	};

	void ImportHDRI(const char* HDRIfilePath, Cubemap& DestinationCubeMap, VKAPP::RendererContext& RendererContext);
}