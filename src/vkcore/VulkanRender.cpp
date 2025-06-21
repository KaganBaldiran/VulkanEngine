#include "VulkanRender.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanWindow.hpp"

void VKCORE::AllocateFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects)
{
    VkSemaphoreCreateInfo SemaphoreCreateInfo{};
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (auto &SyncObjects : DestinationObjects)
    {
        VkFenceCreateInfo FenceCreateInfo{};
        FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        FenceCreateInfo.flags = SyncObjects.FenceCreateFlag;

        if (vkCreateSemaphore(LogicalDevice, &SemaphoreCreateInfo, nullptr, &SyncObjects.ImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(LogicalDevice, &SemaphoreCreateInfo, nullptr, &SyncObjects.RenderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(LogicalDevice, &FenceCreateInfo, nullptr, &SyncObjects.Fence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create the semaphores and the fence!");
        }
    }
}

void VKCORE::DestroyFrameSyncObjects(VkDevice& LogicalDevice, std::vector<FrameSyncObjects>& DestinationObjects)
{
    for (auto& SyncObjects : DestinationObjects)
    {
        vkDestroySemaphore(LogicalDevice, SyncObjects.ImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(LogicalDevice, SyncObjects.RenderFinishedSemaphore, nullptr);
        vkDestroyFence(LogicalDevice, SyncObjects.Fence, nullptr);
    }
}

void VKCORE::SubmitQueue(
    VkQueue &Queue,
    const std::vector<VkSemaphore>& WaitSemaphores,
    const std::vector<VkPipelineStageFlags>& WaitDstStageMask,
    const std::vector<VkCommandBuffer>& CommandBuffers,
    const std::vector<VkSemaphore>& SignalSemaphores,
    VkFence Fence
)
{
    VkSubmitInfo SubmitInfo{};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(WaitSemaphores.size());
    SubmitInfo.pWaitSemaphores = WaitSemaphores.empty() ? nullptr : WaitSemaphores.data();
    SubmitInfo.pWaitDstStageMask = WaitDstStageMask.empty() ? nullptr : WaitDstStageMask.data();
    SubmitInfo.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());
    SubmitInfo.pCommandBuffers = CommandBuffers.empty() ? nullptr : CommandBuffers.data();
    SubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(SignalSemaphores.size());
    SubmitInfo.pSignalSemaphores = SignalSemaphores.empty() ? nullptr : SignalSemaphores.data();

    if (vkQueueSubmit(Queue, 1, &SubmitInfo, Fence) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }
}

VkResult VKCORE::PresentQueue(
    VkQueue& Queue,
    const std::vector<VkSwapchainKHR> &SwapChains,
    const std::vector<VkSemaphore>& WaitSemaphores,
    const std::vector<uint32_t>& ImageIndices,
    const std::vector<VkCommandBuffer>& CommandBuffers
)
{
    VkPresentInfoKHR PresentInfo{};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = static_cast<uint32_t>(WaitSemaphores.size());
    PresentInfo.pWaitSemaphores = WaitSemaphores.data();

    PresentInfo.swapchainCount = static_cast<uint32_t>(SwapChains.size());
    PresentInfo.pSwapchains = SwapChains.data();
    PresentInfo.pImageIndices = ImageIndices.data();
    PresentInfo.pResults = nullptr;
    return vkQueuePresentKHR(Queue, &PresentInfo);
}

void VKCORE::RenderFrame(
    VkDevice &LogicalDevice,
    VkQueue& GraphicsQueue,
    VkQueue& PresentQueue,
    std::vector<VkCommandBuffer>& CommandBuffers,
    std::multimap<int, std::function<void(VkCommandBuffer& CurrentCommandBuffer,uint32_t CurrentImageIndex, uint32_t CurrentFrame)>> CommandBufferRecords,
    std::function<void()> OnSwapChainRecreate,
    std::vector<VKCORE::FrameSyncObjects>& SyncObjects,
    SwapChain& DestinationSwapChain,
    Window& window,
    uint32_t &CurrentFrame,
    uint32_t MaxImagesInFlight
)
{
    auto& CommandBuffer = CommandBuffers[CurrentFrame];
    auto& SyncObject = SyncObjects[CurrentFrame];
    vkWaitForFences(LogicalDevice, 1, &SyncObject.Fence, VK_TRUE, UINT64_MAX);

    uint32_t ImageIndex;
    VkResult Result = vkAcquireNextImageKHR(LogicalDevice, DestinationSwapChain.swapChain, UINT64_MAX, SyncObject.ImageAvailableSemaphore, VK_NULL_HANDLE, &ImageIndex);

    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        OnSwapChainRecreate();
        return;
    }
    else if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    vkResetFences(LogicalDevice, 1, &SyncObject.Fence);

    vkResetCommandBuffer(CommandBuffer, 0);
    for (auto& [InlineIndex,Record] : CommandBufferRecords)
    {
        Record(CommandBuffer,ImageIndex,CurrentFrame);
    }

    VKCORE::SubmitQueue(
        GraphicsQueue,
        { SyncObject.ImageAvailableSemaphore }, 
        { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        { CommandBuffer }, 
        { SyncObject.RenderFinishedSemaphore }, 
        SyncObject.Fence
    );

    Result = VKCORE::PresentQueue(
        PresentQueue,
        {DestinationSwapChain.swapChain},
        {SyncObject.RenderFinishedSemaphore},
        { ImageIndex },
        {CommandBuffer}
    );

    if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR || window.FrameBufferResizedCallback)
    {
        OnSwapChainRecreate();
        window.FrameBufferResizedCallback = false;
        return;
    }
    else if (Result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    CurrentFrame = (CurrentFrame + 1) % MaxImagesInFlight;
}

void VKCORE::DynamicRenderingPass::AppendAttachment(
    VkImageView imageView,
    VkImageLayout imageLayout,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    const VkClearValue& clearValue
)
{
    VkRenderingAttachmentInfo NewAttachmentInfo{};
    NewAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    NewAttachmentInfo.loadOp = loadOp;
    NewAttachmentInfo.storeOp = storeOp;
    NewAttachmentInfo.imageView = imageView;
    NewAttachmentInfo.imageLayout = imageLayout;
    NewAttachmentInfo.clearValue = clearValue;
    if (imageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
        imageLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
    {
        RenderingColorAttachments.push_back(NewAttachmentInfo);
    }
    else if (imageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ||
             imageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
             imageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
             imageLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
    {
        DepthAttachment = NewAttachmentInfo;
        HaveDepthAttachment = true;
    }
    else if (imageLayout == VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL)
    {
        StencilAttachment = NewAttachmentInfo;
    }
}

void VKCORE::DynamicRenderingPass::BeginRendering(const VkCommandBuffer &CommandBuffer,VkRect2D &RenderArea)
{
    VkRenderingInfo RenderingInfo{};
    RenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    RenderingInfo.pColorAttachments = this->RenderingColorAttachments.data();
    RenderingInfo.pDepthAttachment = HaveDepthAttachment ? &DepthAttachment : nullptr;
    RenderingInfo.colorAttachmentCount = static_cast<uint32_t>(RenderingColorAttachments.size());
    RenderingInfo.layerCount = 1;
    RenderingInfo.renderArea = RenderArea;
    RenderingInfo.viewMask = 0;
    RenderingInfo.flags = 0;

    vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
}

void VKCORE::DynamicRenderingPass::EndRendering(const VkCommandBuffer& CommandBuffer)
{
    vkCmdEndRendering(CommandBuffer);
}
