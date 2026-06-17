#ifndef vulkanBuffer_h
#define vulkanBuffer_h

#include "gpu/vulkanCommon.h"

class VulkanDevice;

struct AllocatedBuffer {
	vma::UniqueBuffer buffer;
	vma::UniqueAllocation allocation;
	vma::AllocationInfo info{};

	VulkanDevice* device = nullptr;

	AllocatedBuffer() = default;
	AllocatedBuffer(AllocatedBuffer&&) = default;
	AllocatedBuffer& operator=(AllocatedBuffer&&) = default;
	AllocatedBuffer(const AllocatedBuffer&) = delete;
	AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

	vk::Buffer getBuffer() const { return buffer.get(); }
};

AllocatedBuffer createBuffer(VulkanDevice& device, size_t allocSize, vk::BufferUsageFlags usage, vma::AllocationCreateFlags flags);
void destroyBuffer(AllocatedBuffer& buffer);

#endif /* vulkanBuffer_h */
