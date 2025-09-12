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
    DeviceContext.Create(DeviceCreateInfo, Surface.Handle, Instance.instance);

    SwapChain.Create(DeviceContext.PhysicalDevice, DeviceContext.LogicalDevice, Surface.Handle, Window.window);
    QueueFamilyIndices = RENDERER_CORE::FindQueueFamilies(DeviceContext.PhysicalDevice, Surface.Handle);

    CommandPool.Create(QueueFamilyIndices.GraphicsFamily.value(), DeviceContext.LogicalDevice);

    //Layout needed for the scene descriptor sets
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.CreateLayout(DeviceContext.LogicalDevice);
    SceneDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, SceneDescriptorSetLayout.Handle);

    //Layout needed for the indirect descriptor sets
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 2, VK_SHADER_STAGE_VERTEX_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 4, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    //IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 5, VK_SHADER_STAGE_VERTEX_BIT);
    IndirectDescriptorSetLayout.CreateLayout(DeviceContext.LogicalDevice);
    IndirectDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, IndirectDescriptorSetLayout.Handle);

    //Layout needed for the texture index descriptor sets
    TextureIndicesDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    TextureIndicesDescriptorSetLayout.CreateLayout(DeviceContext.LogicalDevice);
    TextureIndicesDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, TextureIndicesDescriptorSetLayout.Handle);

    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 3, VK_SHADER_STAGE_FRAGMENT_BIT);
    LightingPassLayout.CreateLayout(DeviceContext.LogicalDevice);

    //Quad buffer
    RENDERER_CORE::UploadDataToDeviceLocalBuffer(
        DeviceContext.LogicalDevice,
        DeviceContext.PhysicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        QuadVertices,
        sizeof(QuadVertices),
        QuadVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    //Cube buffer
    RENDERER_CORE::UploadDataToDeviceLocalBuffer(
        DeviceContext.LogicalDevice,
        DeviceContext.PhysicalDevice,
        CommandPool.commandPool,
        DeviceContext.GraphicsQueue,
        skyboxVertices,
        sizeof(skyboxVertices),
        CubeVertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    QuadVertexDescription.SetBindingDescription(0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX);
    QuadVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
    QuadVertexDescription.AppendAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3);

    CubeVertexDescription.SetBindingDescription(0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX);
    CubeVertexDescription.AppendAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

    DepthImageFormat = RENDERER_CORE::FindSupportedFormat(DeviceContext.PhysicalDevice, { VK_FORMAT_D32_SFLOAT,VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    ///Geometry buffer pass vertex shader data creation
    GbufferVertexShaderModule.CompileOrLoadShaderModule(
        "Shaders\\GeometryBufferShader.vert",
        "Shaders\\GeometryBufferShaderVert.spv",
        shaderc_vertex_shader,
        "GbufferVertexShader",
        DeviceContext.LogicalDevice
    );

    ///Geometry buffer pass fragment shader data creation
    GbufferFragmentShaderModule.CompileOrLoadShaderModule(
        "Shaders\\GeometryBufferShader.frag",
        "Shaders\\GeometryBufferShaderFrag.spv",
        shaderc_fragment_shader,
        "GbufferFragmentShader",
        DeviceContext.LogicalDevice
    );
    
    ///Deferred shading pass vertex shader data creation
    LightingVertexShaderModule.CompileOrLoadShaderModule(
        "Shaders\\LightingPassShader.vert",
        "Shaders\\LightingPassShaderVert.spv",
        shaderc_vertex_shader,
        "DeferredShadingVertexShader",
        DeviceContext.LogicalDevice
    );

    ///Deferred shading pass fragment shader data creation
    LightingFragmentShaderModule.CompileOrLoadShaderModule(
        "Shaders\\LightingPassShader.frag",
        "Shaders\\LightingPassShaderFrag.spv",
        shaderc_fragment_shader,
        "DeferredShadingFragmentShader",
        DeviceContext.LogicalDevice
    );

    SingleTimeCommandFence.Create(DeviceContext.LogicalDevice,0);
    //RENDERER_CORE::AllocateCommandBuffers(CommandPool.commandPool,DeviceContext.LogicalDevice,SingleTimeCommandBuffers,)
   
    CreateHDRIrenderPassResources();
    this->IsDestroyed = false;
    this->DestructionPriority = 0;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::RendererContext::Destroy()
{
    if (IsDestroyed) return;

    GbufferVertexShaderModule.Destroy(DeviceContext.LogicalDevice);
    GbufferFragmentShaderModule.Destroy(DeviceContext.LogicalDevice);
    for (auto& [ID, Pipeline] : TextureDescriptorPipelines)
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            Pipeline[i].Destroy(DeviceContext.LogicalDevice);
        }
    }
    SingleTimeCommandFence.Destroy(DeviceContext.LogicalDevice);
    LightingVertexShaderModule.Destroy(DeviceContext.LogicalDevice);
    LightingFragmentShaderModule.Destroy(DeviceContext.LogicalDevice);
    LightingPassLayout.Destroy(DeviceContext.LogicalDevice);
    HDRIrenderPassLayout.Destroy(DeviceContext.LogicalDevice);
    TextureIndicesDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    HDRIrenderPassDescriptorPool.Destroy(DeviceContext.LogicalDevice);
    HDRIrenderGraphicsPipeline.Destroy(DeviceContext.LogicalDevice);
    HDRIconvoluteGraphicsPipeline.Destroy(DeviceContext.LogicalDevice);
    QuadVertexBuffer.Destroy(DeviceContext.LogicalDevice);
    CubeVertexBuffer.Destroy(DeviceContext.LogicalDevice);
    SceneDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    IndirectDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    CommandPool.Destroy(DeviceContext.LogicalDevice);
    Surface.Destroy(Instance.instance);
    SwapChain.Destroy(DeviceContext.LogicalDevice);
    DeviceContext.Destroy();
    Instance.Destroy();
    Window.Destroy();
    IsDestroyed = true;

    std::cout << "Renderer context destroyed!" << std::endl;
}
void RENDERER::RendererContext::WaitDeviceIdle()
{
    vkDeviceWaitIdle(DeviceContext.LogicalDevice);
}
void RENDERER::RendererContext::CreateHDRIrenderPassResources()
{
    ///HDRI render pass descriptor set
    HDRIrenderPassDescriptorPool.Create(
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1} },
        1,
        DeviceContext.LogicalDevice
    );

    HDRIrenderPassLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    HDRIrenderPassLayout.CreateLayout(DeviceContext.LogicalDevice);

    HDRIrenderPassDescriptorSets.resize(1);
    RENDERER_CORE::AllocateDescriptorSets(
        DeviceContext.LogicalDevice, 
        1, 
        HDRIrenderPassDescriptorPool.Handle, 
        {HDRIrenderPassLayout.Handle},
        HDRIrenderPassDescriptorSets
    );

    ///HDRI render pass vertex shader
    RENDERER_CORE::ShaderModule HDRIrenderVertexShader;
    HDRIrenderVertexShader.CompileOrLoadShaderModule(
        "Shaders\\HDRIrenderShader.vert",
        "Shaders\\HDRIrenderVertexShader.spv",
        shaderc_vertex_shader,
        "HDRIrenderVertexShader",
        DeviceContext.LogicalDevice
    );

    ///HDRI render pass fragment shader
    RENDERER_CORE::ShaderModule HDRIrenderFragmentShader;
    HDRIrenderFragmentShader.CompileOrLoadShaderModule(
        "Shaders\\HDRIrenderShader.frag",
        "Shaders\\HDRIrenderFragmentShader.spv",
        shaderc_fragment_shader,
        "HDRIrenderFragmentShader",
        DeviceContext.LogicalDevice
    );

    ///HDRI render pass pipeline
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
    PipelineCreateInfo.DescriptorSetLayouts = { HDRIrenderPassLayout.Handle };
    PipelineCreateInfo.PushConstantRanges = { HDRIrenderPassPushConstantsRange };
    HDRIrenderGraphicsPipeline.Create(PipelineCreateInfo, DeviceContext.LogicalDevice);

    HDRIrenderVertexShader.Destroy(DeviceContext.LogicalDevice);
    HDRIrenderFragmentShader.Destroy(DeviceContext.LogicalDevice);

    ///HDRI convolution pass vertex shader
    RENDERER_CORE::ShaderModule HDRIconvolutionVertexShader;
    HDRIconvolutionVertexShader.CompileOrLoadShaderModule(
        "shaders\\HDRIconvolutionShader.vert",
        "shaders\\HDRIconvolutionVertexShader.spv",
        shaderc_vertex_shader,
        "HDRIconvolutionVertexShader",
        DeviceContext.LogicalDevice
    );

    ///HDRI convolution pass fragment shader
    RENDERER_CORE::ShaderModule HDRIconvolutionFragmentShader;
    HDRIconvolutionFragmentShader.CompileOrLoadShaderModule(
        "shaders\\HDRIconvolutionShader.frag",
        "shaders\\HDRIconvolutionFragmentShader.spv",
        shaderc_fragment_shader,
        "HDRIconvolutionFragmentShader",
        DeviceContext.LogicalDevice
    );

    ///HDRI convolution pass pipeline
    PipelineCreateInfo.ShaderModules = { {&HDRIconvolutionVertexShader,VK_SHADER_STAGE_VERTEX_BIT} ,{&HDRIconvolutionFragmentShader,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.PushConstantRanges = {};
    HDRIconvoluteGraphicsPipeline.Create(PipelineCreateInfo, DeviceContext.LogicalDevice);

    HDRIconvolutionVertexShader.Destroy(DeviceContext.LogicalDevice);
    HDRIconvolutionFragmentShader.Destroy(DeviceContext.LogicalDevice);
}

std::array<RENDERER_CORE::GraphicsPipeline,3>* RENDERER::RendererContext::CreateTextureDescriptorPipelines(VkDescriptorSetLayout Layout, uint32_t MaxTextureCount)
{
    auto& Iterator = TextureDescriptorPipelines.find(MaxTextureCount);
    if (Iterator != TextureDescriptorPipelines.end())
    {
        return &TextureDescriptorPipelines[MaxTextureCount];
    }

    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PushConstantRange.size = 2 * sizeof(glm::mat4);
    PushConstantRange.offset = 0;

    std::vector<VkFormat> ColorAttachmentsFormats = { GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SFLOAT };
    //std::vector<VkFormat> ColorAttachmentsFormats = { GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R8G8B8A8_UNORM , VK_FORMAT_R8G8B8A8_UNORM };
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
    PipelineCreateInfo.DescriptorSetLayouts = { 
        IndirectDescriptorSetLayout.Handle,
        Layout,
        TextureIndicesDescriptorSetLayout.Handle 
    };
    PipelineCreateInfo.PushConstantRanges = { PushConstantRange };
    RENDERER_CORE::GraphicsPipeline GbufferGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice);

    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    RENDERER_CORE::GraphicsPipeline GbufferGraphicsPipelineDepthDisabled(PipelineCreateInfo, DeviceContext.LogicalDevice);
 
    VkPushConstantRange LightingPassPushConstantRange{};
    LightingPassPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    LightingPassPushConstantRange.size = sizeof(LightingPassUBOdata);
    LightingPassPushConstantRange.offset = 0;

    LatestShadingPassPipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    LatestShadingPassPipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    LatestShadingPassPipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    LatestShadingPassPipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    LatestShadingPassPipelineCreateInfo.RenderPass = nullptr;
    LatestShadingPassPipelineCreateInfo.ScissorOffset = { 0,0 };
    LatestShadingPassPipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    LatestShadingPassPipelineCreateInfo.ViewportMinDepth = 0.0f;
    LatestShadingPassPipelineCreateInfo.ViewportMaxDepth = 1.0f;
    LatestShadingPassPipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    LatestShadingPassPipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { VK_FORMAT_R8G8B8A8_SRGB };
    LatestShadingPassPipelineCreateInfo.ShaderModules = { {&LightingVertexShaderModule,VK_SHADER_STAGE_VERTEX_BIT} ,{&LightingFragmentShaderModule,VK_SHADER_STAGE_FRAGMENT_BIT} };
    LatestShadingPassPipelineCreateInfo.AttributeDescriptions = QuadVertexDescription.AttributeDescriptions;
    LatestShadingPassPipelineCreateInfo.BindingDescription = QuadVertexDescription.BindingDescription;
    LatestShadingPassPipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    LatestShadingPassPipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    LatestShadingPassPipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    LatestShadingPassPipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    LatestShadingPassPipelineCreateInfo.DescriptorSetLayouts = {
        LightingPassLayout.Handle,
        SceneDescriptorSetLayout.Handle,
        TextureIndicesDescriptorSetLayout.Handle,
        Layout 
    };
    LatestShadingPassPipelineCreateInfo.PushConstantRanges = { LightingPassPushConstantRange };
    RENDERER_CORE::GraphicsPipeline ShadingPassGraphicsPipeline(LatestShadingPassPipelineCreateInfo, DeviceContext.LogicalDevice);
    TextureDescriptorPipelines[MaxTextureCount] = { GbufferGraphicsPipelineDepthDisabled,GbufferGraphicsPipeline , ShadingPassGraphicsPipeline };

    return &TextureDescriptorPipelines[MaxTextureCount];
}
