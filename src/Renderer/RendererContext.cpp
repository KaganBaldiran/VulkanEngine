#include "RendererContext.hpp"
#include "../Scene/Mesh.hpp"

static float QuadVertices[] = {
    // positions        // texture Coords
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
};

float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

RENDERER::RendererContext::RendererContext(bool EnableValidationLayers)
{
    Create(EnableValidationLayers);
}

void RENDERER::RendererContext::Create(bool EnableValidationLayers)
{
    RENDERER_CORE::VulkanWindowCreateInfo WindowCreateInfo{};
    WindowCreateInfo.WindowInitialHeight = 800;
    WindowCreateInfo.WindowInitialWidth = 1000;
    WindowCreateInfo.WindowsName = "Hello World";
    Window.Create(WindowCreateInfo);

    RENDERER_CORE::VulkanInstanceCreateInfo InstanceCreateInfo{};
    InstanceCreateInfo.APIVersion = VK_API_VERSION_1_3;
    InstanceCreateInfo.ApplicationName = "Application";
    InstanceCreateInfo.ApplicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    InstanceCreateInfo.EngineName = "No Engine";
    InstanceCreateInfo.EnableValidationLayers = EnableValidationLayers;
    InstanceCreateInfo.ValidationLayersToEnable = { "VK_LAYER_KHRONOS_validation" };
    Instance.Create(InstanceCreateInfo);

    Surface.Create(Instance.instance, Window.window);

    RENDERER_CORE::VulkanDeviceCreateInfo DeviceCreateInfo{};
    DeviceCreateInfo.DeviceExtensionsToEnable = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
    DeviceCreateInfo.QueuePriority = 1.0f;
    DeviceContext.Create(DeviceCreateInfo, Surface.surface, Instance.instance);

    SwapChain.Create(DeviceContext.physicalDevice, DeviceContext.logicalDevice, Surface.surface, Window.window);
    QueueFamilyIndices = RENDERER_CORE::FindQueueFamilies(DeviceContext.physicalDevice, Surface.surface);

    CommandPool.Create(QueueFamilyIndices.GraphicsFamily.value(), DeviceContext.logicalDevice);

    //Layout needed for the scene descriptor sets
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.CreateLayout(DeviceContext.logicalDevice);
    SceneDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, SceneDescriptorSetLayout.descriptorSetLayout);

    //Layout needed for the indirect descriptor sets
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 2, VK_SHADER_STAGE_VERTEX_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 5, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.CreateLayout(DeviceContext.logicalDevice);
    IndirectDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, IndirectDescriptorSetLayout.descriptorSetLayout);

    //Layout needed for the texture index descriptor sets
    TextureIndicesDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    TextureIndicesDescriptorSetLayout.CreateLayout(DeviceContext.logicalDevice);
    TextureIndicesDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, TextureIndicesDescriptorSetLayout.descriptorSetLayout);

    //Quad buffer
    RENDERER_CORE::UploadDataToDeviceLocalBuffer(
        DeviceContext.logicalDevice,
        DeviceContext.physicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        QuadVertices,
        sizeof(QuadVertices),
        QuadVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    //Cube buffer
    RENDERER_CORE::UploadDataToDeviceLocalBuffer(
        DeviceContext.logicalDevice,
        DeviceContext.physicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        skyboxVertices,
        sizeof(skyboxVertices),
        CubeVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    DepthImageFormat = RENDERER_CORE::FindSupportedFormat(DeviceContext.physicalDevice, { VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    GbufferVertexShaderModule.Create("Shaders\\GeometryBufferShader.vert", "Shaders\\GeometryBufferShaderVert.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);
    GbufferFragmentShaderModule.Create("Shaders\\GeometryBufferShader.frag", "Shaders\\GeometryBufferShaderFrag.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);

    CreateHDRIrenderPassResources();
    this->IsDestroyed = false;
    this->DestructionPriority = 0;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::RendererContext::Destroy()
{
    if (IsDestroyed) return;

    GbufferVertexShaderModule.Destroy(DeviceContext.logicalDevice);
    GbufferFragmentShaderModule.Destroy(DeviceContext.logicalDevice);
    for (auto& [ID, Pipeline] : GbufferPassPassPipelines)
    {
        Pipeline.Destroy(DeviceContext.logicalDevice);
    }
    HDRIrenderPassLayout.Destroy(DeviceContext.logicalDevice);
    TextureIndicesDescriptorSetLayout.Destroy(DeviceContext.logicalDevice);
    HDRIrenderPassDescriptorPool.Destroy(DeviceContext.logicalDevice);
    HDRIrenderGraphicsPipeline.Destroy(DeviceContext.logicalDevice);
    HDRIconvoluteGraphicsPipeline.Destroy(DeviceContext.logicalDevice);
    QuadVertexBuffer.Destroy(DeviceContext.logicalDevice);
    CubeVertexBuffer.Destroy(DeviceContext.logicalDevice);
    SceneDescriptorSetLayout.Destroy(DeviceContext.logicalDevice);
    IndirectDescriptorSetLayout.Destroy(DeviceContext.logicalDevice);
    CommandPool.Destroy(DeviceContext.logicalDevice);
    Surface.Destroy(Instance.instance);
    SwapChain.Destroy(DeviceContext.logicalDevice);
    DeviceContext.Destroy();
    Instance.Destroy();
    Window.Destroy();
    IsDestroyed = true;

    std::cout << "Renderer context destroyed!" << std::endl;
}
void RENDERER::RendererContext::WaitDeviceIdle()
{
    vkDeviceWaitIdle(DeviceContext.logicalDevice);
}
void RENDERER::RendererContext::CreateHDRIrenderPassResources()
{
    //HDRI render pass descriptor set
    HDRIrenderPassDescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1} },
        1,
        DeviceContext.logicalDevice
    );

    HDRIrenderPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    HDRIrenderPassLayout.CreateLayout(DeviceContext.logicalDevice);

    HDRIrenderPassDescriptorSets.resize(1);
    RENDERER_CORE::AllocateDescriptorSets(
        DeviceContext.logicalDevice, 
        1, 
        HDRIrenderPassDescriptorPool.descriptorPool, 
        {HDRIrenderPassLayout.descriptorSetLayout},
        HDRIrenderPassDescriptorSets
    );

    
    //HDRI render pass pipeline
    RENDERER_CORE::ShaderModule HDRIrenderVertexShader("Shaders\\HDRIrenderShader.vert", "Shaders\\HDRIrenderVertexShader.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);
    RENDERER_CORE::ShaderModule HDRIrenderFragmentShader("Shaders\\HDRIrenderShader.frag", "Shaders\\HDRIrenderFragmentShader.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);

    QuadVertexDescription.SetBindingDescription(0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX);
    QuadVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    QuadVertexDescription.AppendAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3);

    CubeVertexDescription.SetBindingDescription(0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX);
    CubeVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

    VkPushConstantRange HDRIrenderPassPushConstantsRange{};
    HDRIrenderPassPushConstantsRange.size = 2 * sizeof(glm::mat4);
    HDRIrenderPassPushConstantsRange.offset = 0;
    HDRIrenderPassPushConstantsRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {&HDRIrenderVertexShader,VK_SHADER_STAGE_VERTEX_BIT} ,{&HDRIrenderFragmentShader,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.AttributeDescriptions = CubeVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = CubeVertexDescription.BindingDescription;
    PipelineCreateInfo.DescriptorSetLayouts = { HDRIrenderPassLayout.descriptorSetLayout };
    PipelineCreateInfo.PushConstantRanges = { HDRIrenderPassPushConstantsRange };
    HDRIrenderGraphicsPipeline.Create(PipelineCreateInfo, DeviceContext.logicalDevice);

    HDRIrenderVertexShader.Destroy(DeviceContext.logicalDevice);
    HDRIrenderFragmentShader.Destroy(DeviceContext.logicalDevice);

    //HDRI convolution pass pipeline
    RENDERER_CORE::ShaderModule HDRIconvolutionVertexShader("shaders\\HDRIconvolutionShader.vert", "shaders\\HDRIconvolutionVertexShader.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);
    RENDERER_CORE::ShaderModule HDRIconvolutionFragmentShader("shaders\\HDRIconvolutionShader.frag", "shaders\\HDRIconvolutionFragmentShader.spv", VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS, DeviceContext.logicalDevice);

    PipelineCreateInfo.ShaderModules = { {&HDRIconvolutionVertexShader,VK_SHADER_STAGE_VERTEX_BIT} ,{&HDRIconvolutionFragmentShader,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.PushConstantRanges = {};
    HDRIconvoluteGraphicsPipeline.Create(PipelineCreateInfo, DeviceContext.logicalDevice);

    HDRIconvolutionVertexShader.Destroy(DeviceContext.logicalDevice);
    HDRIconvolutionFragmentShader.Destroy(DeviceContext.logicalDevice);
}

RENDERER_CORE::GraphicsPipeline* RENDERER::RendererContext::AppendGbufferPassPipeline(VkDescriptorSetLayout Layout, uint32_t MaxTextureCount)
{
    auto& Iterator = GbufferPassPassPipelines.find(MaxTextureCount);
    if (Iterator != GbufferPassPassPipelines.end())
    {
        return &GbufferPassPassPipelines[MaxTextureCount];
    }

    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PushConstantRange.size = 2 * sizeof(glm::mat4);
    PushConstantRange.offset = 0;

    std::vector<VkFormat> ColorAttachmentsFormats = { GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R8G8B8A8_UNORM , VK_FORMAT_R8G8B8A8_UNORM };
    RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {&GbufferVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&GbufferFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = static_cast<uint32_t>(ColorAttachmentsFormats.size());
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = ColorAttachmentsFormats;
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = DepthImageFormat;
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.AttributeDescriptions = SCENE::Vertex3D::GetAttributeDescriptions();
    PipelineCreateInfo.BindingDescription = SCENE::Vertex3D::GetBindingDescription();
    PipelineCreateInfo.DescriptorSetLayouts = { IndirectDescriptorSetLayout.descriptorSetLayout,Layout,TextureIndicesDescriptorSetLayout.descriptorSetLayout };
    PipelineCreateInfo.PushConstantRanges = { PushConstantRange };
    RENDERER_CORE::GraphicsPipeline GbufferGraphicsPipeline(PipelineCreateInfo, DeviceContext.logicalDevice);
    GbufferPassPassPipelines[MaxTextureCount] = GbufferGraphicsPipeline;

    return &GbufferPassPassPipelines[MaxTextureCount];
};
