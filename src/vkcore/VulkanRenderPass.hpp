#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include "VulkanUtils.hpp" 
#include <functional>
#include <vector>
#include <map>

namespace VKCORE
{
    /*
    void CreateRenderPass()
    {
        VkAttachmentDescription ColorAttachment{};
        ColorAttachment.format = SurfaceFormat.format;
        ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription DepthAttachment{};
        DepthAttachment.format = DepthImageFormat;
        DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ColorAttachmentRef{};
        ColorAttachmentRef.attachment = 0;
        ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference DepthAttachmentRef;
        DepthAttachmentRef.attachment = 1;
        DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription Subpass{};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments = &ColorAttachmentRef;
        Subpass.pDepthStencilAttachment = &DepthAttachmentRef;

        VkSubpassDependency ExternalDependency{};
        ExternalDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        ExternalDependency.dstSubpass = 0;
        ExternalDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        ExternalDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        ExternalDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        ExternalDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> Attachments = { ColorAttachment,DepthAttachment };

        VkRenderPassCreateInfo RenderPassCreateInfo{};
        RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        RenderPassCreateInfo.attachmentCount = Attachments.size();
        RenderPassCreateInfo.pAttachments = Attachments.data();
        RenderPassCreateInfo.subpassCount = 1;
        RenderPassCreateInfo.pSubpasses = &Subpass;
        RenderPassCreateInfo.dependencyCount = 1;
        RenderPassCreateInfo.pDependencies = &ExternalDependency;

        if (vkCreateRenderPass(LogicalDevice, &RenderPassCreateInfo, nullptr, &RenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a render pass!");
        }
    }

    void CreateFramebuffers()
    {
        SwapChainFramebuffers.resize(SwapChainImagesViews.size());
        for (size_t i = 0; i < SwapChainImagesViews.size(); i++)
        {
            VkImageView Attachments[] = {
                SwapChainImagesViews[i],
                DepthBufferImageView
            };

            VkFramebufferCreateInfo FramebufferCreateInfo{};
            FramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            FramebufferCreateInfo.renderPass = RenderPass;
            FramebufferCreateInfo.attachmentCount = 2;
            FramebufferCreateInfo.pAttachments = Attachments;
            FramebufferCreateInfo.width = Extent.width;
            FramebufferCreateInfo.height = Extent.height;
            FramebufferCreateInfo.layers = 1;

            if (vkCreateFramebuffer(LogicalDevice, &FramebufferCreateInfo, nullptr, &this->SwapChainFramebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create a swap chain framebuffer!");
            }
        }
    }

   */
}