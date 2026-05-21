#include "MaterialManager.hpp"
#include "../include/stbi/stb_image.h"
#include "../Renderer/Core/VulkanImage.hpp"
#include "../Renderer/Core/VulkanCommandPool.hpp"
#include "../Renderer/RendererContext.hpp"
#include "../Renderer/ResourceManager.hpp"
#include "../Renderer/Core/VulkanSynchoronization.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void RENDERER::TextureManager::AppendImportTask(TextureImportInfo ImportInfo)
{
	this->ImportQueue.push(ImportInfo);
}

RENDERER::TextureManager::TextureManager(RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr)
{
    Create(RendererContext, ResourceManagerPtr);
}

void RENDERER::TextureManager::Create(RENDERER::RendererContext& RendererContext, RENDERER::ResourceManager& ResourceManagerPtr)
{
    this->RendererContextPtr = &RendererContext;
    this->ResourceManagerPtr = &ResourceManagerPtr;
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

        Futures.push_back({ ImportInfo,std::async(std::launch::async, [this,ImportInfo]() -> TextureImportLoad {
            std::shared_ptr<RENDERER_CORE::RawImageData> NewRawImageData = std::make_shared<RENDERER_CORE::RawImageData>();
            auto Result = RENDERER_CORE::ReadTexture(ImportInfo.FileName.c_str(), *NewRawImageData);
            if (Result < 0) {
                LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Failed reading texture[" + ImportInfo.FileName + "].");
                return TextureImportLoad();
            };

            TextureImportLoad Load;
            Load.RawData = NewRawImageData;
            Load.DestinationTextureID = ImportInfo.DestinationTextureID;

            return Load;
        }) });
    }
}

void RENDERER::TextureManager::WaitImportsIdle()
{
    for (auto& [ImportInfo, future] : Futures)
    {
        TextureImportLoad& Load = future.get();
        if (Load.RawData)
        {
            RawImageDatas[ImportInfo.DestinationTextureID] = Load.RawData;
            auto& DestinationTextureData = TextureDatas[ImportInfo.DestinationTextureID];
            DestinationTextureData.Uploaded = std::make_shared<COMMON::AsyncToken>();

            VkImageCreateInfo ImageCreateInfo{};
            ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            ImageCreateInfo.mipLevels = 1;
            ImageCreateInfo.extent.width = static_cast<uint32_t>(Load.RawData->Width);
            ImageCreateInfo.extent.height = static_cast<uint32_t>(Load.RawData->Height);
            ImageCreateInfo.extent.depth = 1;
            ImageCreateInfo.arrayLayers = 1;
            ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            ImageCreateInfo.flags = 0;

            if (vkCreateImage(RendererContextPtr->DeviceContext.LogicalDevice, &ImageCreateInfo, nullptr, &DestinationTextureData.Data.Image) != VK_SUCCESS)
            {
                LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed to create image [(" + std::to_string(Load.RawData->Width) +
                    "x" + std::to_string(Load.RawData->Height) + ")."));
                throw std::runtime_error("Failed to create image!");
            }

            VkMemoryRequirements ImageMemoryRequirements;
            vkGetImageMemoryRequirements(RendererContextPtr->DeviceContext.LogicalDevice, DestinationTextureData.Data.Image, &ImageMemoryRequirements);
            TextureMemoryAllocation AllocatedMemory = AllocateFromTexturePages(
                ImageMemoryRequirements,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                RendererContextPtr->DeviceContext.PhysicalDevice,
                RendererContextPtr->DeviceContext.LogicalDevice
            );

            vkBindImageMemory(
                RendererContextPtr->DeviceContext.LogicalDevice, 
                DestinationTextureData.Data.Image,
                AllocatedMemory.Memory,
                AllocatedMemory.Region.Offset
            );

            DestinationTextureData.Data.ImageView = RENDERER_CORE::CreateImageView(
                DestinationTextureData.Data.Image,
                VK_FORMAT_R8G8B8A8_SRGB, 
                VK_IMAGE_VIEW_TYPE_2D, 
                VK_IMAGE_ASPECT_COLOR_BIT, 
                RendererContextPtr->DeviceContext.LogicalDevice
            );
            RENDERER_CORE::CreateTextureSampler(
                RendererContextPtr->DeviceContext.PhysicalDevice, 
                RendererContextPtr->DeviceContext.LogicalDevice, 
                DestinationTextureData.Data.Sampler,
                VK_FILTER_LINEAR, 
                VK_SAMPLER_ADDRESS_MODE_REPEAT
            );

            std::shared_ptr<RENDERER::DataBlock> TextureDataBlock = std::make_shared<RENDERER::DataBlock>();
            TextureDataBlock->DataPtr = reinterpret_cast<uint8_t*>(Load.RawData->Pixels);
            TextureDataBlock->SizeInBytes = Load.RawData->Width * Load.RawData->Height * 4;
            TextureDataBlock->Deleter = [RawData = Load.RawData]() {};

            VkBufferImageCopy CopyRegion{};
            CopyRegion.bufferOffset = 0;
            CopyRegion.bufferRowLength = 0;
            CopyRegion.bufferImageHeight = 0;

            CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            CopyRegion.imageSubresource.mipLevel = 0;
            CopyRegion.imageSubresource.baseArrayLayer = 0;
            CopyRegion.imageSubresource.layerCount = 1;

            CopyRegion.imageOffset = { 0,0,0 };
            CopyRegion.imageExtent = { 
                static_cast<uint32_t>(Load.RawData->Width),
                static_cast<uint32_t>(Load.RawData->Height),
                1 
            };

            RENDERER_CORE::ImageMetaData MetaData{};
            MetaData.Width = Load.RawData->Width;
            MetaData.Height = Load.RawData->Height;
            MetaData.BytesPerPixel = 4;

            RENDERER_CORE::BarrierState OnCompleteTransition;
            OnCompleteTransition.AccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            OnCompleteTransition.StageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            OnCompleteTransition.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            ResourceManagerPtr->RequestImageCopyOperation(
                { CopyRegion },
                RENDERER_CORE::QUEUE_TYPE_TRANSFER,
                &DestinationTextureData.Data,
                TextureDataBlock,
                MetaData,
                VK_IMAGE_ASPECT_COLOR_BIT,
                3,
                COPY_OPERATION_FLAG_NONE,
                OnCompleteTransition,
                nullptr,
                0,
                &DestinationTextureData.Uploaded,
                1
            );

            ImportRegistries.emplace(std::string(ImportInfo.FileName), ImportInfo.DestinationTextureID);
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

RENDERER::TextureManager::TextureMemoryAllocation RENDERER::TextureManager::AllocateFromTexturePages(
    VkMemoryRequirements MemoryRequirements,
    VkMemoryPropertyFlags Properties,
    VkPhysicalDevice PhysicalDevice,
    VkDevice LogicalDevice
)
{
    TextureMemoryAllocation Result;
    if (!MemoryRequirements.size) return Result;
    uint32_t MemoryTypeIndex = RENDERER_CORE::FindMemoryType(PhysicalDevice, MemoryRequirements.memoryTypeBits, Properties);
    std::vector<TextureMemoryPage>& TargetPages = TexturePages[MemoryTypeIndex];

    for (size_t i = 0; i < TargetPages.size(); i++)
    {
        TextureMemoryPage& Page = TargetPages[i];
        RENDERER_CORE::MemoryRegion AllocatedRegion = Page.Allocator.Suballocate(MemoryRequirements.size, MemoryRequirements.alignment, false);
        if (!AllocatedRegion.Size)
        {
            continue;
        }
        Result.Region = std::move(AllocatedRegion);
        Result.Memory = Page.Memory;
        return Result;
    }
    VkDeviceSize PageAllocationSize = std::max(TextureMemoryPageSize, MemoryRequirements.size);

    VkMemoryAllocateInfo ImageMemoryAllocationInfo{};
    ImageMemoryAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ImageMemoryAllocationInfo.allocationSize = PageAllocationSize;
    ImageMemoryAllocationInfo.memoryTypeIndex = MemoryTypeIndex;

    TextureMemoryPage NewAllocation{};
    NewAllocation.Allocator.Create(PageAllocationSize, 4);
    if (vkAllocateMemory(LogicalDevice, &ImageMemoryAllocationInfo, nullptr, &NewAllocation.Memory) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed to allocate memory for image [size(" +
            std::to_string(MemoryRequirements.size)));
        throw std::runtime_error("Failed to allocate memory for the image");
    }

    RENDERER_CORE::MemoryRegion AllocatedRegion = NewAllocation.Allocator.Suballocate(MemoryRequirements.size, MemoryRequirements.alignment, false);
    Result.Region = std::move(AllocatedRegion);
    Result.Memory = NewAllocation.Memory;
    TargetPages.push_back(std::move(NewAllocation));
    return Result;
}
