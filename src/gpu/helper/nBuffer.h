#ifndef nBuffer_h
#define nBuffer_h

#include "gpu/abstractions/vulkanBuffer.h"
#include "gpu/renderer/frameManager.h"
#include "gpu/vulkanDevice.h"

class NBuffer {
public:
	void init(VulkanDevice& device, size_t allocSize, vk::BufferUsageFlags usage, vma::AllocationCreateFlags flags);
	void cleanup();

	inline void incrementBufferFrame() { index = (index + 1) % FRAMES_IN_FLIGHT; }
	inline AllocatedBuffer& getCurrentBuffer() { return buffers[index]; }
	inline vk::DescriptorBufferInfo& getCurrentBufferInfo() { return bufferInfos[index]; }

private:
	std::array<AllocatedBuffer, FRAMES_IN_FLIGHT> buffers;
	std::array<vk::DescriptorBufferInfo, FRAMES_IN_FLIGHT> bufferInfos;
	uint32_t index = 0;

	VulkanDevice* device = nullptr;
};

#endif
