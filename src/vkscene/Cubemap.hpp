#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vkcore/VulkanImage.hpp"
#include <array>

#include "SceneResource.hpp"

namespace VKAPP
{
	//Forward Decleration
	class RendererContext;
}

namespace VKSCENE
{
	class ResourceDependencyManager;

	class Cubemap : public SceneResource
	{
	public:
		Cubemap(VKAPP::RendererContext& RendererContext, ResourceDependencyManager& DependencyManager,uint32_t Width, uint32_t Height);
		Cubemap() = default;
		void Create(VKAPP::RendererContext& RendererContext, ResourceDependencyManager& DependencyManager,uint32_t Width, uint32_t Height);
		
		void Destroy(VKAPP::RendererContext& RendererContext);

		glm::ivec2 Size;
		VkImage CubemapImage = VK_NULL_HANDLE;
		VkDeviceMemory CubemapImageMemory = VK_NULL_HANDLE;
		VkSampler CubemapSampler = VK_NULL_HANDLE;
		std::array<VkImageView, 6> CubemapRenderImageViews;
		VkImageView CubemapSampleImageView;

		VkImage ConvolutionImage = VK_NULL_HANDLE;
		VkDeviceMemory ConvolutionImageMemory = VK_NULL_HANDLE;
		VkSampler ConvolutionSampler = VK_NULL_HANDLE;
		std::array<VkImageView, 6> ConvolutionRenderImageViews;
		VkImageView ConvolutionSampleImageView;
	private:
		void CreateCubemapTexture(VKAPP::RendererContext& RendererContext);
		void CreateConvolutionTexture(VKAPP::RendererContext& RendererContext);
	};

	void ImportHDRI(const char* HDRIfilePath, Cubemap& DestinationCubeMap, VKAPP::RendererContext& RendererContext);
}