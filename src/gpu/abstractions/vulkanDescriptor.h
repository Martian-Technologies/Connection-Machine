#ifndef vulkanDescriptor_h
#define vulkanDescriptor_h

#include "gpu/vulkanCommon.h"

class VulkanDevice;

class DescriptorLayoutBuilder {
public:
	void addBinding(uint32_t bindingIndex, vk::DescriptorType type);
	vk::UniqueDescriptorSetLayout build(vk::Device device, vk::ShaderStageFlags shaderStages, vk::DescriptorSetLayoutCreateFlags flags = {});

private:
	std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

class DescriptorWriter {
public:
	void writeImage(int bindingIndex, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
	void writeBuffer(int bindingIndex, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

	void updateSet(vk::Device device, vk::DescriptorSet set);

private:
	std::deque<vk::DescriptorImageInfo> imageInfos;
	std::deque<vk::DescriptorBufferInfo> bufferInfos;
	std::vector<vk::WriteDescriptorSet> writes;
};


struct PoolSizeRatio {
	vk::DescriptorType type;
	float ratio;
};

class DescriptorAllocator {
public:
	void init(VulkanDevice& device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios);
	void cleanup();

	void clearDescriptors();
	vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);

private:
	vk::UniqueDescriptorPool pool;
	VulkanDevice* device = nullptr;
};

class GrowableDescriptorAllocator {
public:
	void init(VulkanDevice& device, uint32_t initialSets, const std::vector<PoolSizeRatio>& poolRatios);
	void cleanup();

	vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);

private:
	vk::DescriptorPool getPool();
	vk::UniqueDescriptorPool createPool(uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

private:
	std::vector<PoolSizeRatio> ratios;
	uint32_t setsPerPool = 0;
	std::vector<vk::UniqueDescriptorPool> fullPools;
	std::vector<vk::UniqueDescriptorPool> readyPools;

	VulkanDevice* device = nullptr;
};

#endif /* vulkanDescriptor_h */
