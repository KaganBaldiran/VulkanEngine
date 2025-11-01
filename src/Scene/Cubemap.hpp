#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Renderer/Core/VulkanImage.hpp"
#include "../Common/DestructionQueue.hpp"
#include <array>

#include "../Common/Handle.hpp"

namespace RENDERER
{
	//Forward Decleration
	class RendererContext;
}

namespace SCENE
{
	class ResourceDependencyManager;

	class Cubemap : public COMMON::Handle , public COMMON::Destructible
	{
	public:
		Cubemap(RENDERER::RendererContext& RendererContext,uint32_t Width, uint32_t Height);
		Cubemap() = default;
		void Create(RENDERER::RendererContext& RendererContext,uint32_t Width, uint32_t Height);
		
		void Destroy() override;

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
		void CreateCubemapTexture(RENDERER::RendererContext& RendererContext);
		void CreateConvolutionTexture(RENDERER::RendererContext& RendererContext);

		RENDERER::RendererContext* RendererContext = nullptr;
	};

	void ImportHDRI(const char* HDRIfilePath, Cubemap& DestinationCubeMap, RENDERER::RendererContext& RendererContext);
}