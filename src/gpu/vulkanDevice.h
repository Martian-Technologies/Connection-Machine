#ifndef vulkanDevice_h
#define vulkanDevice_h

#include <volk.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

struct VulkanInstance;
struct BlockTextureManager;

struct QueueInfo {
	VkQueue queue;
	uint32_t index;
};

class VulkanDevice {
public:
	VulkanDevice(VulkanInstance* instance, VkSurfaceKHR surfaceForPresenting);
	~VulkanDevice();

	// queue submission functions
	void waitIdle();
	VkResult submitGraphicsQueue(VkSubmitInfo* submitInfo, VkFence fence);
	VkResult submitPresent(VkPresentInfoKHR* presentInfo);
	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	inline uint32_t getGraphicsQueueIndex() { return graphicsQueue.index; }
	inline uint32_t getPresentQueueIndex() { return presentQueue.index; }

	inline vkb::Device& getDevice() { return device; }
	inline VmaAllocator getAllocator() { return vmaAllocator; }

	inline BlockTextureManager* getBlockTextureManager() { return blockTextureManager.get(); }
	
private:
	void createAllocator();
	void initializeImmediateSubmission();
	
private:
	// Device details
	VkPhysicalDevice physicalDevice;
	vkb::Device device;
	VmaAllocator vmaAllocator;
	VulkanInstance* instance;

	// Queues
	QueueInfo graphicsQueue;
	QueueInfo presentQueue;
	std::mutex queueMux;

	// Immediate submission system
	VkFence immediateFence;
    VkCommandBuffer immediateCommandBuffer;
    VkCommandPool immediateCommandPool;
	std::mutex immediateSubmitMux;

	// Texture
	std::unique_ptr<BlockTextureManager> blockTextureManager;
};

#endif
