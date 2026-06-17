#include "vulkanBuffer.h"

#include "gpu/vulkanDevice.h"

AllocatedBuffer createBuffer(VulkanDevice& device, size_t allocSize, vk::BufferUsageFlags usage, vma::AllocationCreateFlags flags) {
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	vma::AllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = vma::MemoryUsage::eAuto;
	vmaAllocInfo.flags = flags;

	AllocatedBuffer newBuffer;
	newBuffer.device = &device;

	auto result = device.getAllocator().createBufferUnique(bufferInfo, vmaAllocInfo, &newBuffer.info);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create vulkan buffer");
	}
	newBuffer.buffer = std::move(result.value.second);
	newBuffer.allocation = std::move(result.value.first);
	return newBuffer;
}

void destroyBuffer(AllocatedBuffer& buffer) {
	buffer.buffer.reset();
	buffer.allocation.reset();
}
