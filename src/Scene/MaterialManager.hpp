#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <future>
#include <queue>
#include <map>
#include <string>

#include "../Renderer/Core/VulkanImage.hpp"
#include "SceneResource.hpp"

namespace VKAPP
{
	class RendererContext;
}

namespace VKSCENE
{
	class Texture : public SceneResource
	{
	public:
	private:
	};

	struct TextureImportInfo
	{
		std::string FileName;
		uint64_t DestinationTextureID;
	};

	class TextureImportManager
	{
	public:
		TextureImportManager(VKAPP::RendererContext &RendererContext) : RendererContext(&RendererContext)
		{};
		void Destroy();

		void AppendImportTask(TextureImportInfo ImportInfo);
		void SubmitImport();

		std::vector<std::future<void>> Futures;
		std::queue<TextureImportInfo> ImportQueue;

		std::unordered_map<std::string,uint64_t> ImportRegistries;
		std::unordered_map<uint64_t,VKCORE::RawImageData> RawImageDatas;
		std::unordered_map<uint64_t,VKCORE::TextureData> TextureDatas;
	private:
		double StartingTime;
		VKAPP::RendererContext* RendererContext = nullptr;
	};
}