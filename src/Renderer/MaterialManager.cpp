#include "MaterialManager.hpp"
#include "../include/stbi/stb_image.h"
#include "../Renderer/Core/VulkanImage.hpp"
#include "../Renderer/Core/VulkanCommandPool.hpp"
#include "../Renderer/RendererContext.hpp"
#include "../Renderer/Core/VulkanSynchoronization.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void RENDERER::TextureManager::AppendImportTask(TextureImportInfo ImportInfo)
{
	this->ImportQueue.push(ImportInfo);
}

RENDERER::TextureManager::TextureManager(RENDERER::RendererContext& RendererContext)
{
    Create(RendererContext);
}

void RENDERER::TextureManager::Create(RENDERER::RendererContext& RendererContext)
{
    this->RendererContextPtr = &RendererContext;
    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::TextureManager::Destroy()
{
    if (IsDestroyed) return;
    for (auto &[id,TextureDataEntry]:TextureDatas)
    {
        TextureDataEntry.Data.Destroy(this->RendererContextPtr->DeviceContext.LogicalDevice);
    }
    IsDestroyed = true;

    std::cout << "Texture import manager destroyed!" << std::endl;
}

void RENDERER::TextureManager::SubmitImport()
{
    std::vector<uint64_t> ImagesToTransition;
    ImagesToTransition.reserve(ImportQueue.size());

    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto ImportInfo = ImportQueue.front();
        ImportQueue.pop();

        Futures.push_back({ ImportInfo,std::async(std::launch::async, [this,ImportInfo]() -> bool {
            RENDERER_CORE::RawImageData NewRawImageData;
            auto Result = RENDERER_CORE::ReadTexture(ImportInfo.FileName.c_str(), NewRawImageData);
            if (Result < 0) {
                LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Failed reading texture[" + ImportInfo.FileName + "].");
                return false;
            };

            RawImageDatas[ImportInfo.DestinationTextureID] = NewRawImageData;
            auto& DestinationTextureData = TextureDatas[ImportInfo.DestinationTextureID];

            RENDERER_CORE::CommandPool TempCommandPool(
                RendererContextPtr->QueueFamilyIndices.GraphicsFamily.value(),
                RendererContextPtr->DeviceContext.LogicalDevice,
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            );

            RENDERER_CORE::CreateTextureImageAsync(
                NewRawImageData,
                RendererContextPtr->DeviceContext.PhysicalDevice,
                RendererContextPtr->DeviceContext.LogicalDevice,
                TempCommandPool.Handle,
                RendererContextPtr->DeviceContext.GraphicsQueue,
                DestinationTextureData.Data,
                Mutex
            );

            TempCommandPool.Destroy(RendererContextPtr->DeviceContext.LogicalDevice);
            return true;
        }) });

    }
}

void RENDERER::TextureManager::WaitImportsIdle()
{
    for (auto& [ImportInfo, future] : Futures)
    {
        if (future.get())
        {
            ImportRegistries.emplace(std::string(ImportInfo.FileName), ImportInfo.DestinationTextureID);
            
            auto Iterator = TextureDatas.find(ImportInfo.DestinationTextureID);
            if (Iterator == TextureDatas.end()) continue;

            for (size_t i = 0; i < this->DescriptorWriteQueue.size(); i++)
            {
                DescriptorWriteQueue[i].push_back(ImportInfo.DestinationTextureID);
            }
        }
    }
    Futures.clear();

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Textures were imported in: " << DeltaTime << " seconds" << std::endl;
}

void RENDERER::TextureManager::UpdateDescriptors(uint32_t FrameIndex)
{
    auto& CurrentDescriptorWriteList = this->DescriptorWriteQueue[FrameIndex];
    bool ShouldRewrite = RendererContextPtr->CreateTextureDescriptors(CurrentDescriptorWriteList.size(), FrameIndex,true);

    auto& CurrentTexturesDescriptor = RendererContextPtr->TexturesDescriptors[FrameIndex];
    auto& CurrentTextureDescriptorIndexAllocator = RendererContextPtr->TextureDescriptorIndexAllocators[FrameIndex];
    std::vector<RENDERER_CORE::DescriptorSetWriteImage> ImageWrites;
    if (ShouldRewrite)
    {
        for (auto& [TextureID, DataEntry] : TextureDatas)
        {
            auto& Data = DataEntry.Data;
            RENDERER_CORE::DescriptorSetWriteImage NewTextureWrite(
                Data.ImageView,
                Data.Sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                0,
                CurrentTexturesDescriptor.DescriptorSets[0],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                DataEntry.DescriptorSlots[FrameIndex],
                1
            );
            ImageWrites.push_back(std::move(NewTextureWrite));
        }
    }
    for (size_t i = 0; i < CurrentDescriptorWriteList.size(); i++)
    {
        auto TextureDataIterator = TextureDatas.find(CurrentDescriptorWriteList[i]);
        if (TextureDataIterator == TextureDatas.end()) continue;

        auto& Data = TextureDataIterator->second.Data;
        auto& DescriptorSlots = TextureDataIterator->second.DescriptorSlots;

        auto AllocatedRegion = CurrentTextureDescriptorIndexAllocator.Suballocate(1,1,false);
        if (!AllocatedRegion.Size) break;
        DescriptorSlots[FrameIndex] = AllocatedRegion.Offset;

        RENDERER_CORE::DescriptorSetWriteImage NewTextureWrite(
            Data.ImageView,
            Data.Sampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            CurrentTexturesDescriptor.DescriptorSets[0],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            AllocatedRegion.Offset,
            1
        );
        ImageWrites.push_back(std::move(NewTextureWrite));
    }
    
    RENDERER_CORE::WriteDescriptorSets(
        RendererContextPtr->DeviceContext.LogicalDevice,
        {},
        ImageWrites
    );
    CurrentDescriptorWriteList.clear();
}
