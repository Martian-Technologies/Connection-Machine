#include "vulkanBuffer.h"

AllocatedBuffer createBuffer(VulkanDevice* device, size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags) {
	// allocate buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	vmaAllocInfo.flags = flags;
	
	AllocatedBuffer newBuffer;
	newBuffer.device = device;

	// allocate the buffer
	VkResult result = vmaCreateBuffer(device->getAllocator(), &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info);
	if(result != VK_SUCCESS) {
		throwFatalError("failed to create vulkan buffer");
	}
	return newBuffer;
}

void destroyBuffer(AllocatedBuffer& buffer) {
	vmaDestroyBuffer(buffer.device->getAllocator(), buffer.buffer, buffer.allocation);
}
