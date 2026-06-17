#include "nBuffer.h"

void NBuffer::init(VulkanDevice& device, size_t allocSize, vk::BufferUsageFlags usage, vma::AllocationCreateFlags flags) {
	this->device = &device;

	for (uint32_t i = 0; i < buffers.size(); ++i){
		buffers[i] = createBuffer(*this->device, allocSize, usage, flags);
		bufferInfos[i] = vk::DescriptorBufferInfo{
			buffers[i].buffer.get(),
			0,
			allocSize
		};
	}
}

void NBuffer::cleanup() {
	for (auto& buffer : buffers) {
		destroyBuffer(buffer);
	}
}
