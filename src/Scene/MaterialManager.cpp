#include "MaterialManager.hpp"
#include "../include/stbi/stb_image.h"
#include "../Renderer/Core/VulkanImage.hpp"
#include "../Renderer/Core/VulkanCommandPool.hpp"
#include "../Renderer/RendererContext.hpp"
#include "../Renderer/Core/VulkanSynchoronization.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void SCENE::TextureImportManager::AppendImportTask(TextureImportInfo ImportInfo)
{
	this->ImportQueue.push(ImportInfo);
}

void SCENE::TextureImportManager::Destroy()
{
    for (auto &[id,Data]:TextureDatas)
    {
        Data.Destroy(this->RendererContext->DeviceContext.logicalDevice);
    }
}

void SCENE::TextureImportManager::SubmitImport()
{
    std::mutex Mutex;
    std::vector<uint64_t> ImagesToTransition;
    ImagesToTransition.reserve(ImportQueue.size());

    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto ImportInfo = ImportQueue.front();
        ImportQueue.pop();

        Futures.push_back({ ImportInfo,std::async(std::launch::async, [&Mutex,this,ImportInfo]() -> bool {
            RENDERER_CORE::RawImageData NewRawImageData;
            auto Result = RENDERER_CORE::ReadTexture(ImportInfo.FileName.c_str(), NewRawImageData);
            if (Result < 0) {
                LOG_FILE(GLOBAL_LOG_FILE_PATH, VKCOMMON::LOG_SEVERITY_ERROR, "Failed reading texture[" + ImportInfo.FileName + "].");
                return false;
            };

            RawImageDatas[ImportInfo.DestinationTextureID] = NewRawImageData;
            auto& DestinationTextureData = TextureDatas[ImportInfo.DestinationTextureID];

            RENDERER_CORE::CommandPool TempCommandPool(
                RendererContext->QueueFamilyIndices.GraphicsFamily.value(),
                RendererContext->DeviceContext.logicalDevice,
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            );

            RENDERER_CORE::CreateTextureImageAsync(
                NewRawImageData,
                RendererContext->DeviceContext.physicalDevice,
                RendererContext->DeviceContext.logicalDevice,
                TempCommandPool.commandPool,
                RendererContext->DeviceContext.GraphicsQueue,
                DestinationTextureData,
                Mutex
            );

            TempCommandPool.Destroy(RendererContext->DeviceContext.logicalDevice);
            return true;
        }) });

    }

    for (auto& [ImportInfo,future] : Futures)
    {
        if (future.get())
        {
            ImportRegistries.emplace(std::string(ImportInfo.FileName), ImportInfo.DestinationTextureID);
            ImagesToTransition.push_back(ImportInfo.DestinationTextureID);
        }
    }
    Futures.clear();

    RENDERER_CORE::PipelineBarrier2 PipelineBarrier;
    for (auto& ImageID : ImagesToTransition)
    {
        auto Iterator = TextureDatas.find(ImageID);
        if (Iterator == TextureDatas.end()) continue;
        PipelineBarrier.AppendImageMemoryBarrier(
            Iterator->second.Image,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    auto TransitionImages = [&PipelineBarrier](VkCommandBuffer& CommandBuffer) {
        PipelineBarrier.ExecutePipelineBarrier(CommandBuffer);
    };

    RENDERER_CORE::ExecuteSingleTimeCommand(
        RendererContext->DeviceContext.logicalDevice,
        TransitionImages,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Textures were imported in: " << DeltaTime << " seconds" << std::endl;
}

