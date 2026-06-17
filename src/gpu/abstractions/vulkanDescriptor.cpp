#include "vulkanDescriptor.h"

#include "gpu/vulkanDevice.h"

void DescriptorLayoutBuilder::addBinding(uint32_t bindingIndex, vk::DescriptorType type) {
	vk::DescriptorSetLayoutBinding binding{};
	binding.binding = bindingIndex;
	binding.descriptorCount = 1;
	binding.descriptorType = type;

	bindings.push_back(binding);
}

vk::UniqueDescriptorSetLayout DescriptorLayoutBuilder::build(vk::Device device, vk::ShaderStageFlags shaderStages, vk::DescriptorSetLayoutCreateFlags flags) {
	for (auto& binding : bindings) {
		binding.stageFlags |= shaderStages;
	}

	if (bindings.size() == 0) {
		logWarning("Attempting to create descriptor set layout with 0 bindings", "Vulkan");
	}

	vk::DescriptorSetLayoutCreateInfo info{};
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();
	info.flags = flags;

	auto result = device.createDescriptorSetLayoutUnique(info);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create descriptor set layout");
	}
	return std::move(result.value);
}

void DescriptorWriter::writeImage(int bindingIndex, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type) {
	vk::DescriptorImageInfo& info = imageInfos.emplace_back(vk::DescriptorImageInfo{
		sampler,
		image,
		layout
	});

	vk::WriteDescriptorSet write{};
	write.dstBinding = bindingIndex;
	write.dstSet = nullptr;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;
	writes.push_back(write);
}

void DescriptorWriter::writeBuffer(int bindingIndex, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type) {
	vk::DescriptorBufferInfo& info = bufferInfos.emplace_back(vk::DescriptorBufferInfo{
		buffer,
		offset,
		size
	});

	vk::WriteDescriptorSet write{};
	write.dstBinding = bindingIndex;
	write.dstSet = nullptr;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;
	writes.push_back(write);
}

void DescriptorWriter::updateSet(vk::Device device, vk::DescriptorSet set) {
	for (vk::WriteDescriptorSet& write : writes) {
		write.dstSet = set;
	}

	device.updateDescriptorSets(writes, nullptr);
}

// ============================= DESCRIPTOR ALLOCATOR ===============================================
void DescriptorAllocator::init(VulkanDevice& device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios) {
	this->device = &device;

	std::vector<vk::DescriptorPoolSize> poolSizes;
	for (const PoolSizeRatio& ratio : poolRatios) {
		poolSizes.push_back({ratio.type, uint32_t(ratio.ratio * maxSets)});
	}

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	auto result = device.getDevice().createDescriptorPoolUnique(poolInfo);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create descriptor pool");
	}
	pool = std::move(result.value);
}

void DescriptorAllocator::cleanup() {
	pool.reset();
}

void DescriptorAllocator::clearDescriptors() {
	device->getDevice().resetDescriptorPool(pool.get(), {});
}

vk::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.descriptorPool = pool.get();
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	vk::DescriptorSet set;
	(void)device->getDevice().allocateDescriptorSets(&allocInfo, &set);

	return set;
}

void GrowableDescriptorAllocator::init(VulkanDevice& device, uint32_t initialSets, const std::vector<PoolSizeRatio>& poolRatios) {
	this->device = &device;

	ratios.insert(ratios.begin(), poolRatios.begin(), poolRatios.end());
	setsPerPool = initialSets;

	readyPools.push_back(createPool(setsPerPool, ratios));
}

void GrowableDescriptorAllocator::cleanup() {
	readyPools.clear();
	fullPools.clear();
}

vk::DescriptorSet GrowableDescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
	vk::DescriptorPool pool = getPool();

	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	vk::DescriptorSet descriptorSet;
	vk::Result result = device->getDevice().allocateDescriptorSets(&allocInfo, &descriptorSet);
	if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool) {
		// pool used; move into fullPools
		auto it = std::find_if(readyPools.begin(), readyPools.end(), [pool](const vk::UniqueDescriptorPool& p){ return p.get() == pool; });
		if (it != readyPools.end()) {
			fullPools.push_back(std::move(*it));
			readyPools.erase(it);
		}
		pool = getPool();

		allocInfo.descriptorPool = pool;
		vk::Result result2 = device->getDevice().allocateDescriptorSets(&allocInfo, &descriptorSet);
		if (result2 != vk::Result::eSuccess) {
			logError("Failed to allocate descriptor set!", "Vulkan");
		}
	}

	// pool stays in readyPools
	return descriptorSet;
}

const unsigned int MAX_SETS_PER_POOL = 4092;

vk::DescriptorPool GrowableDescriptorAllocator::getPool() {
	if (!readyPools.empty()) {
		return readyPools.back().get();
	}
	auto newPool = createPool(setsPerPool, ratios);

	setsPerPool = setsPerPool * 1.5f;
	if (setsPerPool > MAX_SETS_PER_POOL){
		setsPerPool = MAX_SETS_PER_POOL;
	}
	readyPools.push_back(std::move(newPool));
	return readyPools.back().get();
}

vk::UniqueDescriptorPool GrowableDescriptorAllocator::createPool(uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
	std::vector<vk::DescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(vk::DescriptorPoolSize(ratio.type, uint32_t(ratio.ratio * setCount)));
	}

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.maxSets = setCount;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	auto result = device->getDevice().createDescriptorPoolUnique(poolInfo);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create descriptor pool");
	}
	return std::move(result.value);
}
