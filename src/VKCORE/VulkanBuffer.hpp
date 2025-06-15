#pragma once
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>
#include <vector>

namespace VKCORE
{
	/// <summary>
	/// Represents a Vulkan buffer and its associated device memory.
	/// </summary>
	struct Buffer
	{
		VkBuffer BufferObject;
		VkDeviceMemory BufferMemory;

		void Destroy(VkDevice &LogicalDevice);
	};

	uint32_t FindMemoryType(VkPhysicalDevice& PhysicalDevice, uint32_t TypeFilter, VkMemoryPropertyFlags Properties);

	/// <summary>
	/// Creates a Vulkan buffer with the specified size, usage, and memory properties, and stores it in the provided destination buffer object.
	/// </summary>
	/// <param name="Size">The size, in bytes, of the buffer to create.</param>
	/// <param name="Usage">A bitmask specifying the intended usage of the buffer (VkBufferUsageFlags).</param>
	/// <param name="Properties">A bitmask specifying the desired memory properties for the buffer (VkMemoryPropertyFlags).</param>
	/// <param name="DestinationBuffer">A reference to a Buffer object where the created buffer will be stored.</param>
	void CreateBuffer(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties, Buffer& DestinationBuffer);
	void CopyBuffer(
		VkBuffer SourceBuffer,
		VkBuffer DestinationBuffer,
		VkDeviceSize Size,
		VkDevice& LogicalDevice,
		VkCommandPool& CommandPool,
		VkQueue& Queue
	);

	/// <summary>
	/// Creates a staging buffer for data transfer operations in Vulkan.
	/// </summary>
	/// <param name="PhysicalDevice">Reference to the Vulkan physical device used to determine memory properties.</param>
	/// <param name="LogicalDevice">Reference to the Vulkan logical device used to create the buffer.</param>
	/// <param name="Size">The size, in bytes, of the buffer to create.</param>
	/// <returns>A Buffer object representing the created staging buffer.</returns>
	void CreateStagingBuffer(VkPhysicalDevice& PhysicalDevice, VkDevice& LogicalDevice, VkDeviceSize Size,Buffer &DestinationBuffer);

	/// <summary>
	/// Uploads data to a device-local Vulkan buffer using a staging buffer and command queue.
	/// </summary>
	/// <param name="LogicalDevice">The Vulkan logical device used for buffer operations.</param>
	/// <param name="PhysicalDevice">The Vulkan physical device associated with the logical device.</param>
	/// <param name="CommandPool">The command pool from which temporary command buffers are allocated.</param>
	/// <param name="Queue">The Vulkan queue used to submit copy commands.</param>
	/// <param name="Data">Pointer to the source data to upload.</param>
	/// <param name="Size">The size, in bytes, of the data to upload.</param>
	/// <param name="DestinationBuffer">Reference to the destination buffer that will receive the uploaded data.</param>
	/// <param name="UsageFlags">Buffer usage flags specifying how the destination buffer will be used.</param>
	void UploadDataToDeviceLocalBuffer(
		VkDevice LogicalDevice,
		VkPhysicalDevice PhysicalDevice,
		VkCommandPool CommandPool,
		VkQueue Queue,
		const void* Data,
		VkDeviceSize Size,
		VKCORE::Buffer& DestinationBuffer,
		VkBufferUsageFlags UsageFlags
	);
}
