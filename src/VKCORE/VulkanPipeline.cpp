#include "VulkanPipeline.hpp"

VKCORE::GraphicsPipeline::GraphicsPipeline(GraphicsPipelineCreateInfo& CreateInfo, VkDevice& LogicalDevice)
{
    auto& ShaderModules = CreateInfo.ShaderModules;
    std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
    ShaderStages.resize(ShaderModules.size());
    for (size_t ModuleIndex = 0; ModuleIndex < ShaderModules.size(); ModuleIndex++)
    {
        auto& Module = ShaderModules[ModuleIndex];
        VkPipelineShaderStageCreateInfo& ShaderStageCreateInfo = ShaderStages[ModuleIndex];
        ShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStageCreateInfo.stage = Module.Usage;
        ShaderStageCreateInfo.module = Module.Module->Module;
        ShaderStageCreateInfo.pName = "main";
    }

    auto& DynamicStates = CreateInfo.DynamicStates;

    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo{};
    DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(DynamicStates.size());
    DynamicStateCreateInfo.pDynamicStates = DynamicStates.empty() ? nullptr : DynamicStates.data();

    auto BindingDescription = CreateInfo.BindingDescription;
    auto AttributeDescriptions = CreateInfo.AttributeDescriptions;

    VkPipelineVertexInputStateCreateInfo VertexInputCreateInputInfo{};
    VertexInputCreateInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputCreateInputInfo.vertexBindingDescriptionCount = 1;
    VertexInputCreateInputInfo.pVertexBindingDescriptions = &BindingDescription;
    VertexInputCreateInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(AttributeDescriptions.size());
    VertexInputCreateInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.empty() ? nullptr : AttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo{};
    InputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyCreateInfo.topology = CreateInfo.Topology;
    InputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport Viewport{};
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = static_cast<float>(CreateInfo.ViewportWidth);
    Viewport.height = static_cast<float>(CreateInfo.ViewportHeight);
    Viewport.minDepth = CreateInfo.ViewportMinDepth;
    Viewport.maxDepth = CreateInfo.ViewportMaxDepth;

    VkRect2D Scissor{};
    Scissor.offset = CreateInfo.ScissorOffset;
    Scissor.extent = CreateInfo.ScissorExtent;

    VkPipelineViewportStateCreateInfo ViewportStateCreateInfo{};
    ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateCreateInfo.viewportCount = 1;
    ViewportStateCreateInfo.pViewports = &Viewport;
    ViewportStateCreateInfo.scissorCount = 1;
    ViewportStateCreateInfo.pScissors = &Scissor;

    VkPipelineRasterizationStateCreateInfo RasterizerStateCreateInfo{};
    RasterizerStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizerStateCreateInfo.depthClampEnable = VK_FALSE;
    RasterizerStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizerStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizerStateCreateInfo.lineWidth = 1.0f;
    RasterizerStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    RasterizerStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterizerStateCreateInfo.depthBiasEnable = VK_FALSE;
    RasterizerStateCreateInfo.depthBiasConstantFactor = 0.0f;
    RasterizerStateCreateInfo.depthBiasClamp = 0.0f;
    RasterizerStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo MultiSamplingCreateInfo{};
    MultiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;
    MultiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultiSamplingCreateInfo.minSampleShading = 1.0f;
    MultiSamplingCreateInfo.pSampleMask = nullptr;
    MultiSamplingCreateInfo.alphaToCoverageEnable = VK_FALSE;
    MultiSamplingCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_TRUE;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineDepthStencilStateCreateInfo DepthStencilStateCreateInfo{};
    DepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    DepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    DepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    DepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    DepthStencilStateCreateInfo.minDepthBounds = 0.0f;
    DepthStencilStateCreateInfo.maxDepthBounds = 1.0f;
    DepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    DepthStencilStateCreateInfo.front = {};
    DepthStencilStateCreateInfo.back = {};

    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo{};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    ColorBlendStateCreateInfo.attachmentCount = 1;
    ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachment;
    ColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo{};
    PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(CreateInfo.DescriptorSetLayouts.size());
    PipelineLayoutCreateInfo.pSetLayouts = CreateInfo.DescriptorSetLayouts.empty() ? nullptr : CreateInfo.DescriptorSetLayouts.data();
    PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    PipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutCreateInfo, nullptr, &Layout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineCreateInfo.stageCount = static_cast<uint32_t>(ShaderStages.size());
    PipelineCreateInfo.pStages = ShaderStages.empty() ? nullptr : ShaderStages.data();
    PipelineCreateInfo.pVertexInputState = &VertexInputCreateInputInfo;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyCreateInfo;
    PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
    PipelineCreateInfo.pRasterizationState = &RasterizerStateCreateInfo;
    PipelineCreateInfo.pMultisampleState = &MultiSamplingCreateInfo;
    PipelineCreateInfo.pDepthStencilState = &DepthStencilStateCreateInfo;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
    PipelineCreateInfo.pDynamicState = DynamicStates.empty() ? nullptr : &DynamicStateCreateInfo;
    PipelineCreateInfo.layout = Layout;

    VkPipelineRenderingCreateInfo RenderingCreateInfo{};
    RenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    RenderingCreateInfo.colorAttachmentCount = CreateInfo.DynamicRenderingColorAttachmentCount;
    RenderingCreateInfo.pColorAttachmentFormats = CreateInfo.DynamicRenderingColorAttachmentsFormats.empty() ? nullptr : CreateInfo.DynamicRenderingColorAttachmentsFormats.data();
    RenderingCreateInfo.depthAttachmentFormat = CreateInfo.DynamicRenderingDepthAttachmentFormat;

    if (CreateInfo.EnableDynamicRendering)
    {
        PipelineCreateInfo.pNext = &RenderingCreateInfo;
        PipelineCreateInfo.renderPass = VK_NULL_HANDLE;
        PipelineCreateInfo.subpass = 0;
    }
    else
    {
        PipelineCreateInfo.pNext = nullptr;
        PipelineCreateInfo.renderPass = *CreateInfo.RenderPass;
        PipelineCreateInfo.subpass = 0;
    }

    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Error creating the graphics pipeline!");
    }
}

void VKCORE::GraphicsPipeline::Destroy(VkDevice& LogicalDevice)
{
    vkDestroyPipeline(LogicalDevice, this->pipeline, nullptr);
}
