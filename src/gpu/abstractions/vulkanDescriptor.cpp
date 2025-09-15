#include "vulkanDescriptor.h"

#include "gpu/vulkanDevice.h"

void DescriptorLayoutBuilder::addBinding(uint32_t bindingIndex, VkDescriptorType type) {
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = bindingIndex;
	binding.descriptorCount = 1;
	binding.descriptorType = type;

	bindings.push_back(binding);
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags) {
	// set shader stage for all bindings
	for (auto& binding : bindings) {
		binding.stageFlags |= shaderStages;
	}

	if (bindings.size() == 0) {
		logWarning("Attempting to create descriptor set layout with 0 bindings", "Vulkan");
	}

	// Create descriptor set layout
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();
	info.flags = flags;

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);

	return layout;
}

void DescriptorWriter::writeImage(int bindingIndex, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {
	// create image info
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	// create write
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = bindingIndex;
	write.dstSet = VK_NULL_HANDLE; // left empty for now until the write
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;
	writes.push_back(write);
}

void DescriptorWriter::writeBuffer(int bindingIndex, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
	// create buffer info
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
	});

	// create write
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = bindingIndex;
	write.dstSet = VK_NULL_HANDLE; // left empty for now until the write
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;
	writes.push_back(write);
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
	for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

// ============================= DESCRIPTOR ALLOCATOR ===============================================
void DescriptorAllocator::init(VulkanDevice* device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios) {
	this->device = device;

	// Get actual pool sizes
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (const PoolSizeRatio& ratio : poolRatios) {
		poolSizes.push_back({ratio.type, uint32_t(ratio.ratio * maxSets)});
	}

	// create pool
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::cleanup() {
	vkDestroyDescriptorPool(device->getDevice(), pool, nullptr);
}

void DescriptorAllocator::clearDescriptors() {
	vkResetDescriptorPool(device->getDevice(), pool, 0);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
	VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    vkAllocateDescriptorSets(device->getDevice(), &allocInfo, &set);

	return set;
}

void GrowableDescriptorAllocator::init(VulkanDevice* device, uint32_t initialSets, const std::vector<PoolSizeRatio>& poolRatios) {
	this->device = device;

	// add ratios
	ratios.insert(ratios.begin(), poolRatios.begin(), poolRatios.end());
	setsPerPool = initialSets;

	// add starting pool
	readyPools.push_back(getPool());
}

void GrowableDescriptorAllocator::cleanup() {
	// destroy all pools
	for (auto pool : readyPools) {
		vkDestroyDescriptorPool(device->getDevice(), pool, nullptr);
	}
	for (auto pool : fullPools) {
		vkDestroyDescriptorPool(device->getDevice(), pool, nullptr);
	}
}

VkDescriptorSet GrowableDescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
	VkDescriptorPool pool = getPool();

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = nullptr;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet descriptorSet;
	VkResult result = vkAllocateDescriptorSets(device->getDevice(), &allocInfo, &descriptorSet);
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL){
		// pool is full, make new one
		fullPools.push_back(pool);
		pool = getPool();

		// allocate again
		allocInfo.descriptorPool = pool;
		VkResult result = vkAllocateDescriptorSets(device->getDevice(), &allocInfo, &descriptorSet);
		if (result != VK_SUCCESS) {
			logError("Failed to allocate descriptor set!", "Vulkan");
		}
	}

	// return pool
	readyPools.push_back(pool);
	return descriptorSet;
}

const unsigned int MAX_SETS_PER_POOL = 4092;

VkDescriptorPool GrowableDescriptorAllocator::getPool() {
	VkDescriptorPool pool;
	// grab a pool or create a new one
	if (!readyPools.empty()) {
		pool = readyPools.back();
		readyPools.pop_back();
	} else {
		pool = createPool(setsPerPool, ratios);

		setsPerPool = setsPerPool * 1.5f;
		if (setsPerPool > MAX_SETS_PER_POOL){
			setsPerPool = MAX_SETS_PER_POOL;
		}
	}

	return pool;
}

VkDescriptorPool GrowableDescriptorAllocator::createPool(uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize(ratio.type, uint32_t(ratio.ratio * setCount)));
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.maxSets = setCount;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &newPool);
	return newPool;
}
