#ifndef frameManager_h
#define frameManager_h

#include "util/lifetimeExtender.h"
#include <vk_mem_alloc.h>

class VulkanDevice;

constexpr unsigned int FRAMES_IN_FLIGHT = 2;

struct Frame {
	void init(VulkanDevice* device);
	void cleanup();

	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
	VkSemaphore acquireSemaphore;
	VkFence renderFence;
	std::chrono::time_point<std::chrono::system_clock> lastStartTime;

	LifetimeExtender lifetime;

	VulkanDevice* device;
};

class FrameManager {
public:
	void init(VulkanDevice* device);
	void cleanup();

	void incrementFrame();
	float waitForCurrentFrameCompletion();
	void startCurrentFrame();

	inline Frame& getCurrentFrame() { return frames[frameIndex]; }

private:
	std::array<Frame, FRAMES_IN_FLIGHT> frames;
	uint32_t frameNumber = 0;
	uint32_t frameIndex = 0;
};

#endif
