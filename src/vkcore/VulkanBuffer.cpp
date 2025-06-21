#include "VulkanBuffer.hpp"
#include <stdexcept>
#include "VulkanCommandBuffer.hpp"

uint32_t VKCORE::FindMemoryType(VkPhysicalDevice &PhysicalDevice,uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
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

void VKCORE::CreateBuffer(VkPhysicalDevice &PhysicalDevice,VkDevice& LogicalDevice,VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties, Buffer& DestinationBuffer)
{
    VkBufferCreateInfo BufferCreateInfo{};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.size = Size;
    BufferCreateInfo.usage = Usage;
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(LogicalDevice, &BufferCreateInfo, nullptr, &DestinationBuffer.BufferObject) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vertex buffer!");
    }

    VkMemoryRequirements MemoryRequirements{};
    vkGetBufferMemoryRequirements(LogicalDevice, DestinationBuffer.BufferObject, &MemoryRequirements);

    VkMemoryAllocateInfo AllocationInfo{};
    AllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocationInfo.allocationSize = MemoryRequirements.size;
    AllocationInfo.memoryTypeIndex = VKCORE::FindMemoryType(PhysicalDevice,MemoryRequirements.memoryTypeBits, Properties);

    if (vkAllocateMemory(LogicalDevice, &AllocationInfo, nullptr, &DestinationBuffer.BufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory!");
    }

    vkBindBufferMemory(LogicalDevice, DestinationBuffer.BufferObject, DestinationBuffer.BufferMemory, 0);
}

void VKCORE::Buffer::Destroy(VkDevice &LogicalDevice)
{
    if (BufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(LogicalDevice, BufferMemory, nullptr);
        BufferMemory = VK_NULL_HANDLE; 
    }

    if (BufferObject != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(LogicalDevice, BufferObject, nullptr);
        BufferObject = VK_NULL_HANDLE; 
    }
}

void VKCORE::CopyBuffer(
    VkBuffer SourceBuffer, 
    VkBuffer DestinationBuffer, 
    VkDeviceSize Size,
    VkDevice &LogicalDevice,
    VkCommandPool &CommandPool,
    VkQueue & Queue
)
{
    auto CopyCommand = [&](VkCommandBuffer& CommandBuffer) {
        VkBufferCopy CopyRegion{};
        CopyRegion.srcOffset = 0;
        CopyRegion.dstOffset = 0;
        CopyRegion.size = Size;
        vkCmdCopyBuffer(CommandBuffer, SourceBuffer, DestinationBuffer, 1, &CopyRegion);
    };
    VKCORE::ExecuteSingleTimeCommand(LogicalDevice,CopyCommand, CommandPool, Queue);
}

void VKCORE::CreateStagingBuffer(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkDeviceSize Size,VKCORE::Buffer &DestinationBuffer)
{
    VKCORE::CreateBuffer(
        PhysicalDevice,
        LogicalDevice,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        DestinationBuffer
    );
}

void VKCORE::UploadDataToDeviceLocalBuffer(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, VkCommandPool CommandPool, VkQueue Queue, const void* Data, VkDeviceSize Size, VKCORE::Buffer& DestinationBuffer, VkBufferUsageFlags UsageFlags)
{
    VKCORE::CreateBuffer(
        PhysicalDevice,
        LogicalDevice,
        Size,
        UsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        DestinationBuffer
    );
    Buffer StagingBuffer{};
    VKCORE::CreateStagingBuffer(PhysicalDevice, LogicalDevice, Size,StagingBuffer);

    void* DataPtr;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, Size, 0, &DataPtr);
    memcpy(DataPtr, Data, Size);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    VKCORE::CopyBuffer(
        StagingBuffer.BufferObject,
        DestinationBuffer.BufferObject,
        Size,
        LogicalDevice,
        CommandPool,
        Queue
    );

    StagingBuffer.Destroy(LogicalDevice);
}

void VKCORE::UploadDataToExistingDeviceLocalBuffer(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, VkCommandPool CommandPool, VkQueue Queue, const void* Data, VkDeviceSize Size, VKCORE::Buffer& DestinationBuffer, VkBufferUsageFlags UsageFlags)
{
    Buffer StagingBuffer{};
    VKCORE::CreateStagingBuffer(PhysicalDevice, LogicalDevice, Size, StagingBuffer);

    void* DataPtr;
    vkMapMemory(LogicalDevice, StagingBuffer.BufferMemory, 0, Size, 0, &DataPtr);
    memcpy(DataPtr, Data, Size);
    vkUnmapMemory(LogicalDevice, StagingBuffer.BufferMemory);

    VKCORE::CopyBuffer(
        StagingBuffer.BufferObject,
        DestinationBuffer.BufferObject,
        Size,
        LogicalDevice,
        CommandPool,
        Queue
    );

    StagingBuffer.Destroy(LogicalDevice);
}

void VKCORE::PersistentBuffer::Map(VkDevice& LogicalDevice, VkDeviceSize Offset, VkDeviceSize Size, VkMemoryMapFlags Flags)
{
    if (vkMapMemory(LogicalDevice, Buffer.BufferMemory, Offset, Size, Flags, &MappedMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Unable to map memory!");
    }
}
