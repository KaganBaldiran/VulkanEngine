#include "RendererContext.hpp"
#include "../Scene/Mesh.hpp"
#include "../Common/Log.hpp"

#include <vulkan/vk_enum_string_helper.h>

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

RENDERER::RendererContext::RendererContext(
    uint32_t WindowWidth,
    uint32_t WindowHeight,
    const char* WindowName,
    RendererSettings Settings
)
{
    Create(
        WindowWidth,
        WindowHeight,
        WindowName,
        Settings
    );
}

void RENDERER::RendererContext::Create(
    uint32_t WindowWidth,
    uint32_t WindowHeight,
    const char* WindowName,
    RendererSettings Settings
)
{
    RENDERER_CORE::VulkanWindowCreateInfo WindowCreateInfo{};
    WindowCreateInfo.WindowInitialHeight = WindowHeight;
    WindowCreateInfo.WindowInitialWidth = WindowWidth;
    WindowCreateInfo.WindowsName = WindowName;
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
    DeviceCreateInfo.QueuePriority = 1.0f;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        nullptr,
        false      
        });

    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures{};
    AccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    AccelerationStructureFeatures.accelerationStructure = VK_TRUE;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
       VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
       &AccelerationStructureFeatures,
       false,
        [&AccelerationStructureFeatures]() { return AccelerationStructureFeatures.accelerationStructure == VK_TRUE; }
    });

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
       VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
       nullptr,
       false
    });

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        nullptr,
        true
    });

    VkPhysicalDeviceDynamicRenderingFeaturesKHR DynamicRenderingFeatures{};
    DynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    DynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
       VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
       &DynamicRenderingFeatures,
       true,
       [&DynamicRenderingFeatures]() { return DynamicRenderingFeatures.dynamicRendering == VK_TRUE; }
    });
    
    VkPhysicalDeviceSynchronization2Features Synchronization2Features{};
    Synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    Synchronization2Features.synchronization2 = VK_TRUE;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
        std::string(),
        &Synchronization2Features,
        true,
        [&Synchronization2Features]() { return Synchronization2Features.synchronization2 == VK_TRUE; }
    });

    VkPhysicalDeviceTimelineSemaphoreFeatures TimelineFeatures{};
    TimelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    TimelineFeatures.timelineSemaphore = VK_TRUE;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
        std::string(),
        &TimelineFeatures,
        true,
        [&TimelineFeatures]() { return TimelineFeatures.timelineSemaphore == VK_TRUE; }
    });

    DeviceCreateInfo.RequestedDeviceFeatureNodes.insert(DeviceCreateInfo.RequestedDeviceFeatureNodes.end(), Settings.RequestedDeviceFeatureNodes.begin(), Settings.RequestedDeviceFeatureNodes.end());

    VkPhysicalDeviceDescriptorIndexingFeatures PhysicalDeviceDescriptorIndexingFeatures{};
    PhysicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    PhysicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
    PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
    PhysicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

    DeviceCreateInfo.RequestedDeviceFeatureNodes.push_back({
       std::string(),
       & PhysicalDeviceDescriptorIndexingFeatures,
       true,
       [&PhysicalDeviceDescriptorIndexingFeatures]() { return PhysicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray == VK_TRUE && 
       PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound == VK_TRUE || PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount == VK_TRUE
        || PhysicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE || PhysicalDeviceDescriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE; }
    });

    DeviceCreateInfo.FeaturesToEnable = &DynamicRenderingFeatures;
    DeviceContext.Create(DeviceCreateInfo, Surface.Handle, Instance.instance);

    SwapChain.Create(DeviceContext.PhysicalDevice, DeviceContext.LogicalDevice, Surface.Handle, Window.window);
    QueueFamilyIndices = RENDERER_CORE::FindQueueFamilies(DeviceContext.PhysicalDevice, Surface.Handle);

    CommandPool.Create(QueueFamilyIndices.GraphicsFamily.value(), DeviceContext.LogicalDevice);
    PipelineManager.Create(100);
    //Layout needed for the scene descriptor sets
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    SceneDescriptorSetLayout.CreateLayout(DeviceContext.LogicalDevice);
    SceneDescriptorSetLayouts.resize(MAX_FRAMES_IN_FLIGHT, SceneDescriptorSetLayout.Handle);

    //Layout needed for the indirect descriptor sets
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 2, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    IndirectDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 3, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
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

    //Post process pass layout
    PostProcessDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    PostProcessDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    PostProcessDescriptorSetLayout.AppendLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 2, VK_SHADER_STAGE_FRAGMENT_BIT);
    PostProcessDescriptorSetLayout.CreateLayout(DeviceContext.LogicalDevice);

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
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    ///Geometry buffer pass vertex shader module creation
    ShaderManager.AppendShaderModule(
        "GbufferVertexShader",
        "Shaders\\GeometryBufferShader.vert",
        "Shaders\\GeometryBufferShaderVert.spv",
        shaderc_vertex_shader,
        DeviceContext.LogicalDevice
    );

    ///Geometry buffer pass fragment shader module creation
    ShaderManager.AppendShaderModule(
        "GbufferFragmentShader",
        "Shaders\\GeometryBufferShader.frag",
        "Shaders\\GeometryBufferShaderFrag.spv",
        shaderc_fragment_shader,
        DeviceContext.LogicalDevice
    );

    ///Deferred shading pass vertex shader module creation
    ShaderManager.AppendShaderModule(
        "DeferredShadingVertexShader",
        "Shaders\\LightingPassShader.vert",
        "Shaders\\LightingPassShaderVert.spv",
        shaderc_vertex_shader,
        DeviceContext.LogicalDevice
    );

    ///Deferred shading pass fragment shader module creation
    ShaderManager.AppendShaderModule(
        "DeferredShadingFragmentShader",
        "Shaders\\LightingPassShader.frag",
        "Shaders\\LightingPassShaderFrag.spv",
        shaderc_fragment_shader,
        DeviceContext.LogicalDevice
    );

    ///Default post processing pass fragment shader module creation
    ShaderManager.AppendShaderModule(
        "DefaultPostProcessingFragmentShader",
        "Shaders\\DefaultPostProcessingShader.frag",
        "Shaders\\DefaultPostProcessingShader.spv",
        shaderc_fragment_shader,
        DeviceContext.LogicalDevice
    );

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TextureDescriptorIndexAllocators[i].Create(0,1);
        this->TextureDescriptorUpperBounds[i] = 0;
        CreateTextureDescriptors(InitialTextureDescriptorSize, i);
    }
    SingleTimeCommandFence.Create(DeviceContext.LogicalDevice,0);
    //RENDERER_CORE::AllocateCommandBuffers(CommandPool.commandPool,DeviceContext.LogicalDevice,SingleTimeCommandBuffers,)
   
    CreateHDRIrenderPassResources();
    CreatePostProcessingPassPipeline();
    CreateComputePipelines();

    this->IsDestroyed = false;
    this->DestructionPriority = 0;
    COMMON::DestructionQueue::Get()->Register(this);
}

void RENDERER::RendererContext::Destroy()
{
    if (IsDestroyed) return;
    DestroyMeshTextureDescriptors();
    PipelineManager.Destroy(DeviceContext.LogicalDevice);
    ShaderManager.Destroy(DeviceContext.LogicalDevice);

    SingleTimeCommandFence.Destroy(DeviceContext.LogicalDevice);
   
    LightingPassLayout.Destroy(DeviceContext.LogicalDevice);
    HDRIrenderPassLayout.Destroy(DeviceContext.LogicalDevice);
    TextureIndicesDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    SceneDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    PostProcessDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    IndirectDescriptorSetLayout.Destroy(DeviceContext.LogicalDevice);
    
    HDRIrenderPassDescriptorPool.Destroy(DeviceContext.LogicalDevice);
    
    QuadVertexBuffer.Destroy(DeviceContext.LogicalDevice);
    CubeVertexBuffer.Destroy(DeviceContext.LogicalDevice);

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

RENDERER::GPUmemoryStats RENDERER::RendererContext::QueryMemoryStats()
{
    VkPhysicalDeviceMemoryBudgetPropertiesEXT BudgetProperties{};
    BudgetProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2 MemoryProperties{};
    MemoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    MemoryProperties.pNext = &BudgetProperties;
    vkGetPhysicalDeviceMemoryProperties2(DeviceContext.PhysicalDevice, &MemoryProperties);

    GPUmemoryStats Stats{};
    for (size_t i = 0; i < MemoryProperties.memoryProperties.memoryHeapCount; i++)
    {
        if (MemoryProperties.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            Stats.TotalUsedBytes += BudgetProperties.heapUsage[i];
            Stats.TotalBudgetBytes += BudgetProperties.heapBudget[i];
        }
    }
    Stats.UsageRate = static_cast<float>(Stats.TotalUsedBytes) / Stats.TotalBudgetBytes;
    return Stats;
}

bool RENDERER::RendererContext::IsRayQuerySupported()
{
    return DeviceContext.AccelerationStructureFeatures.accelerationStructure && DeviceContext.RayQueryFeatures.rayQuery;
}

bool RENDERER::RendererContext::IsRayTracingPipelineSupported()
{
    return DeviceContext.AccelerationStructureFeatures.accelerationStructure && DeviceContext.RayTracingPipelineFeatures.rayTracingPipeline;
}

bool RENDERER::RendererContext::CreateTextureDescriptors(uint32_t DescriptorCount, uint32_t FrameIndex,bool DestroyPrevious)
{
    bool ShouldRewrite = false;
    auto& CurrentTexturesDescriptor = TexturesDescriptors[FrameIndex];
    auto& CurrentTextureDescriptorUpperBound = TextureDescriptorUpperBounds[FrameIndex];
    auto& CurrentDescriptorIndexAllocator = TextureDescriptorIndexAllocators[FrameIndex];

    if (DescriptorCount > CurrentDescriptorIndexAllocator.GetTotalFreeSpace())
    {
        /*
        std::cout << "CurrentTextureDescriptorUpperBound: " << CurrentTextureDescriptorUpperBound << " DescriptorCount: "
            << DescriptorCount << " CurrentDescriptorIndexAllocator.GetTotalFreeSpace(): " << CurrentDescriptorIndexAllocator.GetTotalFreeSpace() << std::endl;
        */
        if (CurrentTextureDescriptorUpperBound)
        {
            CurrentTexturesDescriptor.Destroy(DeviceContext.LogicalDevice);
            ShouldRewrite = true;
        }

        CurrentTextureDescriptorUpperBound = static_cast<uint32_t>(glm::ceil((float)(DescriptorCount + CurrentTextureDescriptorUpperBound) / (float)TextureDescriptorBlockSize)) * TextureDescriptorBlockSize;
        CurrentDescriptorIndexAllocator.Allocate(CurrentTextureDescriptorUpperBound - CurrentDescriptorIndexAllocator.GetCapacity());
        //std::cout << "Creating texture descriptor set with upper bound: " << CurrentTextureDescriptorUpperBound << " CurrentDescriptorIndexAllocator.GetCapacity(): " << CurrentDescriptorIndexAllocator.GetCapacity() << "\n";

        CurrentTexturesDescriptor.DescriptorPool.Create(
            { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,CurrentTextureDescriptorUpperBound} },
            1,
            DeviceContext.LogicalDevice,
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
            DeviceContext.LogicalDevice,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            &BindingFlags
        );

        RENDERER_CORE::AllocateDescriptorSets(
            DeviceContext.LogicalDevice,
            CurrentTexturesDescriptor.DescriptorSets.size(),
            CurrentTexturesDescriptor.DescriptorPool.Handle,
            CurrentTexturesDescriptor.Layout.Handle,
            CurrentTexturesDescriptor.DescriptorSets.data()
        );

        CreateTextureDescriptorPipelines(CurrentTexturesDescriptor.Layout, CurrentTextureDescriptorUpperBound, FrameIndex);
        UpdateCustomPipelines(CurrentTexturesDescriptor.Layout,FrameIndex);
    }
    return ShouldRewrite;
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
    RENDERER_CORE::ShaderModule* HDRIrenderPassVertexShaderPtr = ShaderManager.AppendShaderModule(
        "HDRIrenderVertexShader",
        "Shaders\\HDRIrenderShader.vert",
        "Shaders\\HDRIrenderVertexShader.spv",
        shaderc_vertex_shader,
        DeviceContext.LogicalDevice
    );

    ///HDRI render pass fragment shader
    RENDERER_CORE::ShaderModule* HDRIrenderPassFragmentShaderPtr = ShaderManager.AppendShaderModule(
        "HDRIrenderFragmentShader",
        "Shaders\\HDRIrenderShader.frag",
        "Shaders\\HDRIrenderFragmentShader.spv",
        shaderc_fragment_shader,
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
    PipelineCreateInfo.ShaderModules = { {HDRIrenderPassVertexShaderPtr,VK_SHADER_STAGE_VERTEX_BIT} ,{HDRIrenderPassFragmentShaderPtr,VK_SHADER_STAGE_FRAGMENT_BIT} };
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
    PipelineCreateInfo.DescriptorSetLayouts = { HDRIrenderPassLayout };
    PipelineCreateInfo.PushConstantRanges = { HDRIrenderPassPushConstantsRange };
    DefaultPipelines.HDRIrender = PipelineManager.AppendGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice).second;

    ///HDRI convolution pass vertex shader
    RENDERER_CORE::ShaderModule* HDRIconvolutionVertexShaderPtr = ShaderManager.AppendShaderModule(
        "HDRIconvolutionVertexShader",
        "shaders\\HDRIconvolutionShader.vert",
        "shaders\\HDRIconvolutionVertexShader.spv",
        shaderc_vertex_shader,
        DeviceContext.LogicalDevice
    );

    ///HDRI convolution pass fragment shader
    RENDERER_CORE::ShaderModule* HDRIconvolutionFragmentShaderPtr = ShaderManager.AppendShaderModule(
        "HDRIconvolutionFragmentShader",
        "shaders\\HDRIconvolutionShader.frag",
        "shaders\\HDRIconvolutionFragmentShader.spv",
        shaderc_fragment_shader,
        DeviceContext.LogicalDevice
    );

    ///HDRI convolution pass pipeline
    PipelineCreateInfo.ShaderModules = { {HDRIconvolutionVertexShaderPtr,VK_SHADER_STAGE_VERTEX_BIT} ,{HDRIconvolutionFragmentShaderPtr,VK_SHADER_STAGE_FRAGMENT_BIT} };
    //PipelineCreateInfo.PushConstantRanges = {};
    DefaultPipelines.HDRIconvolute = PipelineManager.AppendGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice).second;
}

void RENDERER::RendererContext::CreateTextureDescriptorPipelines(RENDERER_CORE::DescriptorSetLayout& Layout, uint32_t MaxTextureCount,uint32_t FrameIndex)
{
    /*
    std::cout << "Deleted indices: " << DefaultPipelines.GbufferDepthEnabled[FrameIndex] << " "
            << DefaultPipelines.GbufferDepthDisabled[FrameIndex] << " "
            << DefaultPipelines.DeferredShading[FrameIndex] << " "
            << FrameIndex
            << std::endl;
            */
    //PipelineManager.EraseGraphicsPipelineByIndex(DefaultPipelines.GbufferDepthEnabled[FrameIndex], DeviceContext.LogicalDevice);
    //PipelineManager.EraseGraphicsPipelineByIndex(DefaultPipelines.GbufferDepthDisabled[FrameIndex], DeviceContext.LogicalDevice);
    //PipelineManager.EraseGraphicsPipelineByIndex(DefaultPipelines.DeferredShading[FrameIndex], DeviceContext.LogicalDevice);

    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PushConstantRange.size = 2 * sizeof(glm::mat4);
    PushConstantRange.offset = 0;

    RENDERER_CORE::ShaderModule* GbufferVertexShaderPtr = ShaderManager.GetShaderModule("GbufferVertexShader");
    RENDERER_CORE::ShaderModule* GbufferFragmentShaderPtr = ShaderManager.GetShaderModule("GbufferFragmentShader");

    std::vector<VkFormat> ColorAttachmentsFormats = { GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SFLOAT };
    //std::vector<VkFormat> ColorAttachmentsFormats = { GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R16G16B16A16_SFLOAT ,VK_FORMAT_R8G8B8A8_UNORM , VK_FORMAT_R8G8B8A8_UNORM };
    RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {GbufferVertexShaderPtr,VK_SHADER_STAGE_VERTEX_BIT} ,{GbufferFragmentShaderPtr,VK_SHADER_STAGE_FRAGMENT_BIT} };
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
        IndirectDescriptorSetLayout,
        Layout,
        TextureIndicesDescriptorSetLayout 
    };
    PipelineCreateInfo.PushConstantRanges = { PushConstantRange };
    auto GbufferPassGraphicsPipelineIterator = PipelineManager.AppendGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice);
    DefaultPipelines.GbufferDepthEnabled[FrameIndex] = GbufferPassGraphicsPipelineIterator.second;

    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;

    auto GbufferGraphicsPipelineDepthDisabledIterator = PipelineManager.AppendGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice);
    DefaultPipelines.GbufferDepthDisabled[FrameIndex] = GbufferGraphicsPipelineDepthDisabledIterator.second;
 
    VkPushConstantRange LightingPassPushConstantRange{};
    LightingPassPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    LightingPassPushConstantRange.size = sizeof(LightingPassUBOdata);
    LightingPassPushConstantRange.offset = 0;

    RENDERER_CORE::ShaderModule* DeferredShadingVertexShaderPtr = ShaderManager.GetShaderModule("DeferredShadingVertexShader");
    RENDERER_CORE::ShaderModule* DeferredShadingFragmentShaderPtr = ShaderManager.GetShaderModule("DeferredShadingFragmentShader");

    RENDERER_CORE::GraphicsPipelineCreateInfo DeferredShadingPipelineCreateInfo{};
    DeferredShadingPipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    DeferredShadingPipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    DeferredShadingPipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    DeferredShadingPipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    DeferredShadingPipelineCreateInfo.RenderPass = nullptr;
    DeferredShadingPipelineCreateInfo.ScissorOffset = { 0,0 };
    DeferredShadingPipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    DeferredShadingPipelineCreateInfo.ViewportMinDepth = 0.0f;
    DeferredShadingPipelineCreateInfo.ViewportMaxDepth = 1.0f;
    DeferredShadingPipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    DeferredShadingPipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { this->SwapChain.SurfaceFormat.format };
    DeferredShadingPipelineCreateInfo.ShaderModules = { {DeferredShadingVertexShaderPtr,VK_SHADER_STAGE_VERTEX_BIT} ,{DeferredShadingFragmentShaderPtr,VK_SHADER_STAGE_FRAGMENT_BIT} };
    DeferredShadingPipelineCreateInfo.AttributeDescriptions = QuadVertexDescription.AttributeDescriptions;
    DeferredShadingPipelineCreateInfo.BindingDescription = QuadVertexDescription.BindingDescription;
    DeferredShadingPipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    DeferredShadingPipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    DeferredShadingPipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    DeferredShadingPipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    DeferredShadingPipelineCreateInfo.DescriptorSetLayouts = {
        LightingPassLayout,
        SceneDescriptorSetLayout,
        TextureIndicesDescriptorSetLayout,
        Layout 
    };
    DeferredShadingPipelineCreateInfo.PushConstantRanges = { LightingPassPushConstantRange };

    auto DeferredShadingGraphicsPipelineIterator = PipelineManager.AppendGraphicsPipeline(DeferredShadingPipelineCreateInfo, DeviceContext.LogicalDevice);
    DefaultPipelines.DeferredShading[FrameIndex] = DeferredShadingGraphicsPipelineIterator.second;

    /*
    std::cout << "Appended indices: " << DefaultPipelines.GbufferDepthEnabled[FrameIndex] << " "
        << DefaultPipelines.GbufferDepthDisabled[FrameIndex] << " "
        << DefaultPipelines.DeferredShading[FrameIndex] << " "
        << FrameIndex
        << std::endl;
        */
}

void RENDERER::RendererContext::DestroyMeshTextureDescriptors()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        TextureDescriptorIndexAllocators[i].Reset(0);
        TexturesDescriptors[i].Destroy(DeviceContext.LogicalDevice);
    }
}

void RENDERER::RendererContext::CreateComputePipelines()
{
    //Culling compute shader
    ShaderManager.AppendShaderModule(
        "CullingShader",
        "Shaders\\CullingShader.comp.glsl",
        "Shaders\\CullingShader.spv",
        shaderc_compute_shader,
        DeviceContext.LogicalDevice
    );

    size_t CullingPushConstantSize = sizeof(glm::vec4) * 7;
    VkPushConstantRange CullingPushConstantRange{};
    CullingPushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    CullingPushConstantRange.size = CullingPushConstantSize;
    CullingPushConstantRange.offset = 0;

    //Culling compute pipeline
    RENDERER_CORE::ComputePipelineCreateInfo CullingPipelineCreateInfo{};
    CullingPipelineCreateInfo.DescriptorSetLayouts = { IndirectDescriptorSetLayout };
    CullingPipelineCreateInfo.PushConstantRanges = { CullingPushConstantRange };
    CullingPipelineCreateInfo.ComputeShaderModule = ShaderManager.GetShaderModule("CullingShader");
    auto CullingComputePipelineIterator = PipelineManager.AppendComputePipeline(CullingPipelineCreateInfo, DeviceContext.LogicalDevice);
    DefaultPipelines.CullingCompute = CullingComputePipelineIterator.second;

    //////////                                   //////////                                      //////////
   
    //Culling reset compute shader
    ShaderManager.AppendShaderModule(
        "CullingResetShader",
        "Shaders\\CullingResetShader.comp.glsl",
        "Shaders\\CullingResetShader.spv",
        shaderc_compute_shader,
        DeviceContext.LogicalDevice
    );

    VkPushConstantRange CullingResetPushConstantRange{};
    CullingResetPushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    CullingResetPushConstantRange.size = sizeof(unsigned int);
    CullingResetPushConstantRange.offset = 0;

    //Culling reset compute pipeline
    RENDERER_CORE::ComputePipelineCreateInfo CullingResetPipelineCreateInfo{};
    CullingResetPipelineCreateInfo.DescriptorSetLayouts = { IndirectDescriptorSetLayout };
    CullingResetPipelineCreateInfo.PushConstantRanges = { CullingResetPushConstantRange };
    CullingResetPipelineCreateInfo.ComputeShaderModule = ShaderManager.GetShaderModule("CullingResetShader");
    auto CullingResetComputePipelineIterator = PipelineManager.AppendComputePipeline(CullingResetPipelineCreateInfo, DeviceContext.LogicalDevice);
    DefaultPipelines.CullingResetCompute = CullingResetComputePipelineIterator.second;
}

void RENDERER::RendererContext::UpdateCustomPipelines(RENDERER_CORE::DescriptorSetLayout& Layout,uint32_t FrameIndex)
{   
    auto& CurrentCustomPipelines = CustomPipelines[FrameIndex];
    for (auto& [PipelineHandleID,PipelineIndex] : CurrentCustomPipelines)
    {
        RENDERER_CORE::GraphicsPipelineEntry* PipelinePtr = PipelineManager.GetGraphicsPipeline(PipelineIndex);
        if (PipelinePtr)
        {
            //There isn't a two way connection between the custom pipelines and the context so can't convey the 
            //index of the newly created pipelines back to custom pipeline bodies.
            //Instead keep the index intact and create on place.
            size_t PreviousHash = PipelinePtr->Pipeline.GetHash();
            RENDERER_CORE::GraphicsPipelineCreateInfo& CustomPipelineCreateInfo = PipelinePtr->PipelineCreateInfo;
            CustomPipelineCreateInfo.DescriptorSetLayouts[3] = Layout;
            size_t PipelineHash = CustomPipelineCreateInfo.Hash();

            PipelinePtr->Pipeline.Destroy(DeviceContext.LogicalDevice);
            RENDERER_CORE::GraphicsPipeline NewPipeline;
            NewPipeline.Create(CustomPipelineCreateInfo, DeviceContext.LogicalDevice);

            PipelineManager.GraphicsPipelineHashTable.erase(PreviousHash);
            PipelineManager.GraphicsPipelineHashTable.insert({ NewPipeline.GetHash(),PipelineIndex });

            //Construct the new entry.
            RENDERER_CORE::GraphicsPipelineEntry NewPipelineEntry{};
            NewPipelineEntry.Pipeline = std::move(NewPipeline);
            NewPipelineEntry.PipelineCreateInfo = std::move(CustomPipelineCreateInfo);
            NewPipelineEntry.IncreaseReference();
            PipelineManager.GraphicPipelines[PipelineIndex] = std::move(NewPipelineEntry);
        }
    }
}

void RENDERER::RendererContext::CreatePostProcessingPassPipeline()
{
    VkPushConstantRange PushConstantRange{};
    PushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    PushConstantRange.size = sizeof(PostProcessingPassPushConstantData);
    PushConstantRange.offset = 0;

    RENDERER_CORE::ShaderModule* DeferredShadingVertexShaderPtr = ShaderManager.GetShaderModule("DeferredShadingVertexShader");
    RENDERER_CORE::ShaderModule* PostProcessingFragmentShaderPtr = ShaderManager.GetShaderModule("DefaultPostProcessingFragmentShader");

    RENDERER_CORE::GraphicsPipelineCreateInfo PipelineCreateInfo{};
    PipelineCreateInfo.EnableDynamicRendering = VK_TRUE;
    PipelineCreateInfo.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineCreateInfo.ViewportWidth = static_cast<float>(SwapChain.Extent.width);
    PipelineCreateInfo.ViewportHeight = static_cast<float>(SwapChain.Extent.height);
    PipelineCreateInfo.DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
    PipelineCreateInfo.ShaderModules = { {DeferredShadingVertexShaderPtr,VK_SHADER_STAGE_VERTEX_BIT} ,{PostProcessingFragmentShaderPtr,VK_SHADER_STAGE_FRAGMENT_BIT} };
    PipelineCreateInfo.DynamicRenderingColorAttachmentCount = 1;
    PipelineCreateInfo.DynamicRenderingColorAttachmentsFormats = { this->SwapChain.SurfaceFormat.format};
    PipelineCreateInfo.DynamicRenderingDepthAttachmentFormat = VK_FORMAT_UNDEFINED;
    PipelineCreateInfo.RenderPass = nullptr;
    PipelineCreateInfo.ScissorOffset = { 0,0 };
    PipelineCreateInfo.ScissorExtent = { SwapChain.Extent.width ,SwapChain.Extent.height };
    PipelineCreateInfo.ViewportMinDepth = 0.0f;
    PipelineCreateInfo.ViewportMaxDepth = 1.0f;
    PipelineCreateInfo.AttributeDescriptions = QuadVertexDescription.AttributeDescriptions;
    PipelineCreateInfo.BindingDescription = QuadVertexDescription.BindingDescription;
    PipelineCreateInfo.EnableDepthTesting = VK_FALSE;
    PipelineCreateInfo.EnableDepthWriting = VK_FALSE;
    PipelineCreateInfo.DescriptorSetLayouts = {
        PostProcessDescriptorSetLayout
    };
    PipelineCreateInfo.PushConstantRanges = { PushConstantRange };
    DefaultPipelines.PostProcessing = PipelineManager.AppendGraphicsPipeline(PipelineCreateInfo, DeviceContext.LogicalDevice).second;
}
