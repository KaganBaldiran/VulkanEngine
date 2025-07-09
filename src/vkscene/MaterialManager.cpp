#include "MaterialManager.hpp"
#include "../include/stbi/stb_image.h"
#include "../vkcore/VulkanImage.hpp"
#include "../vkcore/VulkanCommandPool.hpp"
#include "../app/RendererContext.hpp"
#include "../vkcore/VulkanSynchoronization.hpp"

void VKSCENE::TextureImportManager::AppendImportTask(TextureImportInfo ImportInfo)
{
	this->ImportQueue.push(ImportInfo);
}

void VKSCENE::TextureImportManager::Destroy()
{
    for (auto &[id,Data]:TextureDatas)
    {
        Data.Destroy(this->RendererContext->DeviceContext.logicalDevice);
    }
}

void VKSCENE::TextureImportManager::SubmitImport()
{
    std::mutex Mutex;
    std::vector<uint64_t> ImagesToTransition;
    ImagesToTransition.reserve(ImportQueue.size());

    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto ImportInfo = std::move(ImportQueue.front());
        ImportQueue.pop();

        auto &DestinationTexture = RawImageDatas[ImportInfo.DestinationTextureID];
        auto &DestinationTextureData = TextureDatas[ImportInfo.DestinationTextureID];
        ImportRegistries.emplace(std::string(ImportInfo.FileName),ImportInfo.DestinationTextureID);
        ImagesToTransition.push_back(ImportInfo.DestinationTextureID);
        
        Futures.push_back(std::async(std::launch::async, [&Mutex,&DestinationTextureData,this,&DestinationTexture,ImportInfo]() {
            auto Result = VKCORE::ReadTexture(ImportInfo.FileName.c_str(), DestinationTexture);
            if (Result < 0) return;

            VKCORE::CommandPool TempCommandPool(
                RendererContext->QueueFamilyIndices.GraphicsFamily.value(),
                RendererContext->DeviceContext.logicalDevice,
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            );

            VKCORE::CreateTextureImageAsync(
                DestinationTexture,
                RendererContext->DeviceContext.physicalDevice,
                RendererContext->DeviceContext.logicalDevice,
                TempCommandPool.commandPool,
                RendererContext->DeviceContext.GraphicsQueue, 
                DestinationTextureData,
                Mutex
            );

            TempCommandPool.Destroy(RendererContext->DeviceContext.logicalDevice);
        }));
    }

    for (auto& future : Futures)
    {
        future.get();
    }
    Futures.clear();

    
    VKCORE::PipelineBarrier2 PipelineBarrier;
    for (auto& ImageID : ImagesToTransition)
    {
        auto& Iterator = TextureDatas.find(ImageID);
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

    VKCORE::ExecuteSingleTimeCommand(
        RendererContext->DeviceContext.logicalDevice,
        TransitionImages,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Textures were imported in: " << DeltaTime << " seconds" << std::endl;
}

