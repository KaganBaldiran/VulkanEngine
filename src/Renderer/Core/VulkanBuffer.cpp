#include "VulkanBuffer.hpp"
#include "VulkanCommandBuffer.hpp"

#include "../../Common/Log.hpp"
#include "../../Common/CommonDefinitions.hpp"

#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

void RENDERER_CORE::UnMapBuffer(VkDevice& LogicalDevice, Buffer& DestinationBuffer)
{
    if (DestinationBuffer.MappedMemory)
    {
        vkUnmapMemory(LogicalDevice, DestinationBuffer.BufferMemory);
        DestinationBuffer.MappedMemory = nullptr;
    }
}

uint32_t RENDERER_CORE::FindMemoryType(VkPhysicalDevice &PhysicalDevice,uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
{
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

    for (size_t i = 0; i < MemoryProperties.memoryTypeCount; i++)
    {
        if (TypeFilter & (1 << i) && (MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
        {
            return i;
        }
    }
}

void RENDERER_CORE::CreateBuffer(
    VkPhysicalDevice &PhysicalDevice,
    VkDevice& LogicalDevice,
    VkDeviceSize Size, 
    VkBufferUsageFlags Usage, 
    VkMemoryPropertyFlags Properties, 
    Buffer& DestinationBuffer,
    VkMemoryAllocateFlags AllocateFlags
)
{
    VkBufferCreateInfo BufferCreateInfo{};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.size = Size;
    BufferCreateInfo.usage = Usage;
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(LogicalDevice, &BufferCreateInfo, nullptr, &DestinationBuffer.BufferObject) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, 
            COMMON::LOG_SEVERITY_ERROR,"Unable to create the buffer [address(" + std::to_string(reinterpret_cast<uintptr_t>(DestinationBuffer.BufferObject)) + ")" + ", size(" + std::to_string(Size) + ")]" + " object.");
        throw std::runtime_error("Failed to create vertex buffer!");
    }

    VkMemoryRequirements MemoryRequirements{};
    vkGetBufferMemoryRequirements(LogicalDevice, DestinationBuffer.BufferObject, &MemoryRequirements);

    VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo{};
    MemoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    MemoryAllocateFlagsInfo.flags = AllocateFlags;

    VkMemoryAllocateInfo AllocationInfo{};
    AllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocationInfo.allocationSize = MemoryRequirements.size;
    AllocationInfo.memoryTypeIndex = RENDERER_CORE::FindMemoryType(PhysicalDevice,MemoryRequirements.memoryTypeBits, Properties);
    AllocationInfo.pNext = &MemoryAllocateFlagsInfo;

    if (vkAllocateMemory(LogicalDevice, &AllocationInfo, nullptr, &DestinationBuffer.BufferMemory) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, "Unable to allocate memory [" + std::to_string(MemoryRequirements.size) + "] for buffer.");
        throw std::runtime_error("Failed to allocate memory!");
    }
    vkBindBufferMemory(LogicalDevice, DestinationBuffer.BufferObject, DestinationBuffer.BufferMemory, 0);

    DestinationBuffer.BarrierState = BarrierState();
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO,
        std::string("Buffer[address(" + std::to_string(reinterpret_cast<uintptr_t>(DestinationBuffer.BufferObject)) + 
            ")" + ", size(" + std::to_string(MemoryRequirements.size) + 
            "), usage(" + string_VkBufferUsageFlags(Usage) +
            "), properties(" + string_VkMemoryPropertyFlags(Properties) + ")] created.")
    );
}

void RENDERER_CORE::DestroyBuffer(VkDevice &LogicalDevice,Buffer &DestinationBuffer)
{
    UnMapBuffer(LogicalDevice, DestinationBuffer);
    auto BufferPtr = reinterpret_cast<uintptr_t>(DestinationBuffer.BufferObject);
    if (DestinationBuffer.BufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(LogicalDevice, DestinationBuffer.BufferMemory, nullptr);
        DestinationBuffer.BufferMemory = VK_NULL_HANDLE;
    }
    if (DestinationBuffer.BufferObject != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(LogicalDevice, DestinationBuffer.BufferObject, nullptr);
        DestinationBuffer.BufferObject = VK_NULL_HANDLE;
    }
    DestinationBuffer.BarrierState = BarrierState();
    LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_INFO, std::string("Buffer[address(" + std::to_string(BufferPtr) + ")] destroyed!"));
}

void RENDERER_CORE::CopyBuffer(
    VkBuffer SourceBuffer, 
    VkBuffer DestinationBuffer, 
    VkDeviceSize Size,
    VkDevice &LogicalDevice,
    VkCommandPool &CommandPool,
    VkQueue & Queue,
    VkDeviceSize SrcOffset,
    VkDeviceSize DstOffset
)
{
    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = SrcOffset;
        CopyRegion.dstOffset = DstOffset;
        CopyRegion.size = Size;
        vkCmdCopyBuffer(CommandBuffer, SourceBuffer, DestinationBuffer, 1, &CopyRegion);
    };
    RENDERER_CORE::ExecuteSingleTimeCommand(LogicalDevice,CopyCommand, CommandPool, Queue);
}

void RENDERER_CORE::CopyBuffer(std::vector<VkBufferCopy> CopyRegions, VkBuffer SourceBuffer, VkBuffer DestinationBuffer, VkDevice& LogicalDevice, VkCommandPool& CommandPool, VkQueue& Queue)
{
    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        vkCmdCopyBuffer(CommandBuffer, SourceBuffer, DestinationBuffer, static_cast<uint32_t>(CopyRegions.size()), CopyRegions.data());
    };
    RENDERER_CORE::ExecuteSingleTimeCommand(LogicalDevice, CopyCommand, CommandPool, Queue);
}

void RENDERER_CORE::CopyBuffer(
    std::vector<BufferCopyInfo> CopyInfos,
    VkDevice& LogicalDevice, 
    VkCommandPool& CommandPool, 
    VkQueue& Queue
)
{
    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        for (auto& CopyInfo : CopyInfos)
        {
            if (CopyInfo.CopyRegions.empty()) continue;
            vkCmdCopyBuffer(CommandBuffer, CopyInfo.SourceBuffer, CopyInfo.DestinationBuffer, static_cast<uint32_t>(CopyInfo.CopyRegions.size()), CopyInfo.CopyRegions.data());
        }
    };
    RENDERER_CORE::ExecuteSingleTimeCommand(LogicalDevice, CopyCommand, CommandPool, Queue);
}

void RENDERER_CORE::CreateStagingBuffer(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkDeviceSize Size,RENDERER_CORE::Buffer &DestinationBuffer)
{
    RENDERER_CORE::CreateBuffer(
        PhysicalDevice,
        LogicalDevice,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        DestinationBuffer
    );
}

void RENDERER_CORE::UploadDataToDeviceLocalBuffer(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, VkCommandPool CommandPool, VkQueue Queue, const void* Data, VkDeviceSize Size, RENDERER_CORE::Buffer& DestinationBuffer, VkBufferUsageFlags UsageFlags)
{
    RENDERER_CORE::CreateBuffer(
        PhysicalDevice,
        LogicalDevice,
        Size,
        UsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        DestinationBuffer
    );
    Buffer StagingBuffer{};
    RENDERER_CORE::CreateStagingBuffer(PhysicalDevice, LogicalDevice, Size,StagingBuffer);

    void* DataPtr;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, Size, 0, &DataPtr);
    memcpy(DataPtr, Data, Size);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    RENDERER_CORE::CopyBuffer(
        StagingBuffer.BufferObject,
        DestinationBuffer.BufferObject,
        Size,
        LogicalDevice,
        CommandPool,
        Queue
    );

    RENDERER_CORE::DestroyBuffer(LogicalDevice, StagingBuffer);
}

void RENDERER_CORE::UploadDataToExistingDeviceLocalBuffer(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, VkCommandPool CommandPool, VkQueue Queue, const void* Data, VkDeviceSize Size, RENDERER_CORE::Buffer& DestinationBuffer, VkBufferUsageFlags UsageFlags)
{
    Buffer StagingBuffer{};
    RENDERER_CORE::CreateStagingBuffer(PhysicalDevice, LogicalDevice, Size, StagingBuffer);

    void* DataPtr;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, Size, 0, &DataPtr);
    memcpy(DataPtr, Data, Size);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    RENDERER_CORE::CopyBuffer(
        StagingBuffer.BufferObject,
        DestinationBuffer.BufferObject,
        Size,
        LogicalDevice,
        CommandPool,
        Queue
    );

    RENDERER_CORE::DestroyBuffer(LogicalDevice, StagingBuffer);
}

VkDeviceAddress RENDERER_CORE::GetBufferDeviceAddress(VkDevice& LogicalDevice,const Buffer& Buffer)
{
    VkBufferDeviceAddressInfo Info{};
    Info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    Info.buffer = Buffer.BufferObject;
    return vkGetBufferDeviceAddress(LogicalDevice,&Info);
}

void RENDERER_CORE::MapBuffer(Buffer& DestinationBuffer, VkDevice& LogicalDevice, VkDeviceSize Offset, VkDeviceSize Size, VkMemoryMapFlags Flags)
{
    if (vkMapMemory(LogicalDevice, DestinationBuffer.BufferMemory, Offset, Size, Flags, &DestinationBuffer.MappedMemory) != VK_SUCCESS)
    {
        LOG_FILE(GLOBAL_LOG_FILE_PATH, COMMON::LOG_SEVERITY_ERROR, std::string("Failed mapping memory [address(" +
            std::to_string(reinterpret_cast<uintptr_t>(DestinationBuffer.BufferMemory)) + ")]"));
        throw std::runtime_error("Unable to map memory!");
    }
}
