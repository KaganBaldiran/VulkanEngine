#include "MaterialManager.hpp"
#include "../include/stbi/stb_image.h"
#include "../Renderer/Core/VulkanImage.hpp"
#include "../Renderer/Core/VulkanCommandPool.hpp"
#include "../Renderer/RendererContext.hpp"
#include "../Renderer/Core/VulkanSynchoronization.hpp"

#include "../Common/Log.hpp"
#include "../Common/CommonDefinitions.hpp"

void SCENE::TextureManager::AppendImportTask(TextureImportInfo ImportInfo)
{
	this->ImportQueue.push(ImportInfo);
}

SCENE::TextureManager::TextureManager(RENDERER::RendererContext& RendererContext)
{
    Create(RendererContext);
}

void SCENE::TextureManager::Create(RENDERER::RendererContext& RendererContext)
{
    this->RendererContext = &RendererContext;
    IsDestroyed = false;
    DestructionPriority = 2;
    COMMON::DestructionQueue::Get()->Register(this);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TextureDescriptorIndexAllocators[i].Create();
        this->TextureDescriptorUpperBounds[i] = 0;
    }
}

void SCENE::TextureManager::Destroy()
{
    if (IsDestroyed) return;

    for (size_t i = 0; i < TexturesDescriptors.size(); i++)
    {
        TexturesDescriptors[i].Destroy(this->RendererContext->DeviceContext.LogicalDevice);
    }
    for (auto &[id,TextureDataEntry]:TextureDatas)
    {
        TextureDataEntry.Data.Destroy(this->RendererContext->DeviceContext.LogicalDevice);
    }
    IsDestroyed = true;

    std::cout << "Texture import manager destroyed!" << std::endl;
}

void SCENE::TextureManager::SubmitImport()
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
                LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Failed reading texture[" + ImportInfo.FileName + "].");
                return false;
            };

            RawImageDatas[ImportInfo.DestinationTextureID] = NewRawImageData;
            auto& DestinationTextureData = TextureDatas[ImportInfo.DestinationTextureID];

            RENDERER_CORE::CommandPool TempCommandPool(
                RendererContext->QueueFamilyIndices.GraphicsFamily.value(),
                RendererContext->DeviceContext.LogicalDevice,
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            );

            RENDERER_CORE::CreateTextureImageAsync(
                NewRawImageData,
                RendererContext->DeviceContext.PhysicalDevice,
                RendererContext->DeviceContext.LogicalDevice,
                TempCommandPool.commandPool,
                RendererContext->DeviceContext.GraphicsQueue,
                DestinationTextureData.Data,
                Mutex
            );

            TempCommandPool.Destroy(RendererContext->DeviceContext.LogicalDevice);
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

    //RENDERER_CORE::PipelineBarrier2 PipelineBarrier;
    for (auto& ImageID : ImagesToTransition)
    {
        auto Iterator = TextureDatas.find(ImageID);
        if (Iterator == TextureDatas.end()) continue;
        /*
        PipelineBarrier.AppendImageMemoryBarrier(
            Iterator->second.Data.Image,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            0,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        Iterator->second.Data.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        */
        for (size_t i = 0; i < this->DescriptorWriteQueue.size(); i++)
        {
            DescriptorWriteQueue[i].push_back(ImageID);
        }
    }

    /*
    auto TransitionImages = [&PipelineBarrier](VkCommandBuffer& CommandBuffer) {
        PipelineBarrier.ExecutePipelineBarrier(CommandBuffer);
    };
    */

    /*
    RENDERER_CORE::ExecuteSingleTimeCommand(
        RendererContext->DeviceContext.logicalDevice,
        TransitionImages,
        RendererContext->CommandPool.commandPool,
        RendererContext->DeviceContext.GraphicsQueue
    );
    */
    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Textures were imported in: " << DeltaTime << " seconds" << std::endl;
}

void SCENE::TextureManager::UpdateDescriptors(uint32_t FrameIndex)
{
    auto& CurrentTextureDescriptorUpperBound = TextureDescriptorUpperBounds[FrameIndex];
    auto& CurrentDescriptorWriteList = this->DescriptorWriteQueue[FrameIndex];
    auto& CurrentTexturesDescriptor = TexturesDescriptors[FrameIndex];
    auto& CurrentTextureDescriptorIndexAllocator = TextureDescriptorIndexAllocators[FrameIndex];

    bool ShouldRewrite = CreateMeshTextureDescriptors(CurrentTextureDescriptorUpperBound + CurrentDescriptorWriteList.size(), FrameIndex);
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
    else
    {
        for (size_t i = 0; i < CurrentDescriptorWriteList.size(); i++)
        {
            auto TextureDataIterator = TextureDatas.find(CurrentDescriptorWriteList[i]);
            if (TextureDataIterator == TextureDatas.end()) continue;

            auto& Data = TextureDataIterator->second.Data;
            auto& DescriptorSlots = TextureDataIterator->second.DescriptorSlots;

            auto AllocatedIndex = CurrentTextureDescriptorIndexAllocator.Suballocate(1);
            DescriptorSlots[FrameIndex] = AllocatedIndex.Offset;

            RENDERER_CORE::DescriptorSetWriteImage NewTextureWrite(
                Data.ImageView,
                Data.Sampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                0,
                CurrentTexturesDescriptor.DescriptorSets[0],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                AllocatedIndex.Offset,
                1
            );
            ImageWrites.push_back(std::move(NewTextureWrite));
        }
    }

    RENDERER_CORE::WriteDescriptorSets(
        RendererContext->DeviceContext.LogicalDevice,
        {},
        ImageWrites
    );
    CurrentDescriptorWriteList.clear();
}

bool SCENE::TextureManager::CreateMeshTextureDescriptors(
    uint32_t DescriptorCount,
    uint32_t FrameIndex
)
{
    bool ShouldRewrite = false;
    auto& CurrentTexturesDescriptor = TexturesDescriptors[FrameIndex];
    auto& CurrentTextureDescriptorUpperBound = TextureDescriptorUpperBounds[FrameIndex];
    if (DescriptorCount > CurrentTextureDescriptorUpperBound)
    {
        if (CurrentTextureDescriptorUpperBound)
        {
            CurrentTexturesDescriptor.Destroy(RendererContext->DeviceContext.LogicalDevice);
            ShouldRewrite = true;
        }

        CurrentTextureDescriptorUpperBound = static_cast<uint32_t>(glm::ceil((float)DescriptorCount / (float)TextureDescriptorBlockSize)) * TextureDescriptorBlockSize;
        std::cout << "Creating texture descriptor set with upper bound: " << CurrentTextureDescriptorUpperBound << "\n";

        CurrentTexturesDescriptor.DescriptorPool.Create(
            { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,CurrentTextureDescriptorUpperBound} },
            1,
            RendererContext->DeviceContext.LogicalDevice,
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
        );

        VkDescriptorBindingFlags LayoutFlags[2] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        };
        VkDescriptorSetLayoutBindingFlagsCreateInfo BindingFlags{};
        BindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        BindingFlags.pBindingFlags = LayoutFlags;
        BindingFlags.bindingCount = 1;

        CurrentTexturesDescriptor.Layout.AppendLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            CurrentTextureDescriptorUpperBound,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT
        );

        CurrentTexturesDescriptor.Layout.CreateLayout(
            RendererContext->DeviceContext.LogicalDevice,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            &BindingFlags
        );

        RENDERER_CORE::AllocateDescriptorSets(
            RendererContext->DeviceContext.LogicalDevice,
            CurrentTexturesDescriptor.DescriptorSets.size(),
            CurrentTexturesDescriptor.DescriptorPool.Handle,
            CurrentTexturesDescriptor.Layout.Handle,
            CurrentTexturesDescriptor.DescriptorSets.data()
        );
        TextureDescriptorsPipelines = RendererContext->CreateTextureDescriptorPipelines(CurrentTexturesDescriptor.Layout.Handle, CurrentTextureDescriptorUpperBound);
        return ShouldRewrite;
    }
    return ShouldRewrite;
}

void SCENE::TextureManager::DestroyMeshTextureDescriptors()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TexturesDescriptors[i].Destroy(RendererContext->DeviceContext.LogicalDevice);
    }
}

