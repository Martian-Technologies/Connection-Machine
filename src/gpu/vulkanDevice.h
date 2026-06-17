#ifndef vulkanDevice_h
#define vulkanDevice_h

#include "gpu/vulkanCommon.h"
#include <VkBootstrap.h>

#include "gpu/renderer/viewport/blockTextureManager.h"

class BlockTextureManager;

struct QueueInfo {
	vk::Queue queue;
	uint32_t index;
};

class VulkanDevice {
public:
	VulkanDevice(vk::SurfaceKHR surfaceForPresenting);
	~VulkanDevice();

	// queue submission functions
	void waitIdle();
	void waitIdleNoMux();
	vk::Result submitGraphicsQueue(const vk::SubmitInfo& submitInfo, vk::Fence fence);
	vk::Result submitPresent(const vk::PresentInfoKHR& presentInfo);
	void immediateSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);

	inline std::mutex& getGraphicsQueueLock() { return queueMux; }
	inline const QueueInfo& getGraphicsQueue() const { return graphicsQueue; }
	inline uint32_t getGraphicsQueueIndex() const { return graphicsQueue.index; }
	inline uint32_t getPresentQueueIndex() const { return presentQueue.index; }

	inline vkb::Device& getVkbDevice() { return vkbDevice; }
	inline const vkb::Device& getVkbDevice() const { return vkbDevice; }
	inline vk::Device getDevice() const { return vk::Device(vkbDevice.device); }
	inline vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice; }
	inline vma::Allocator getAllocator() { return vmaAllocator.get(); }
	inline vma::Allocator getAllocator() const { return vmaAllocator.get(); }

	inline BlockTextureManager& getBlockTextureManager() { return blockTextureManager; }

	vk::SampleCountFlagBits getMaxUsableSampleCount() const { return msaaSamples; }

private:
	void createAllocator();
	void initializeImmediateSubmission();

private:
	vk::PhysicalDevice physicalDevice;
	vkb::Device vkbDevice;
	vma::UniqueAllocator vmaAllocator;

	QueueInfo graphicsQueue;
	QueueInfo presentQueue;
	std::mutex queueMux;

	vk::UniqueFence immediateFence;
	vk::UniqueCommandPool immediateCommandPool;
	vk::CommandBuffer immediateCommandBuffer;
	std::mutex immediateSubmitMux;

	BlockTextureManager blockTextureManager;
	vk::SampleCountFlagBits msaaSamples;
};

#endif /* vulkanDevice_h */
