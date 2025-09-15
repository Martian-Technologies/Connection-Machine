#ifndef vulkanDescriptor_h
#define vulkanDescriptor_h

#include <volk.h>

class VulkanDevice;

class DescriptorLayoutBuilder {
public:
	void addBinding(uint32_t bindingIndex, VkDescriptorType type);
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags = 0);

private:
	std::vector<VkDescriptorSetLayoutBinding> bindings;
};

class DescriptorWriter {
public:
	void writeImage(int bindingIndex, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int bindingIndex, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    void updateSet(VkDevice device, VkDescriptorSet set);

private:
	std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
};


struct PoolSizeRatio {
	VkDescriptorType type;
	float ratio;
};

class DescriptorAllocator {
public:
	void init(VulkanDevice* device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios);
	void cleanup();

	void clearDescriptors();
	VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
	VkDescriptorPool pool;
	VulkanDevice* device;
};

class GrowableDescriptorAllocator {
public:
	void init(VulkanDevice* device, uint32_t initialSets, const std::vector<PoolSizeRatio>& poolRatios);
	void cleanup();

	VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
	VkDescriptorPool getPool();
	VkDescriptorPool createPool(uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

private:
	std::vector<PoolSizeRatio> ratios;
	uint32_t setsPerPool;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;

	VulkanDevice* device;
};

#endif
