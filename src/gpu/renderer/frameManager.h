#ifndef frameManager_h
#define frameManager_h

#include "util/lifetimeExtender.h"
#include "gpu/vulkanCommon.h"

class VulkanDevice;

constexpr unsigned int FRAMES_IN_FLIGHT = 2;

struct Frame {
	Frame(VulkanDevice& device);

	vk::UniqueCommandPool commandPool;
	vk::CommandBuffer mainCommandBuffer;
	vk::UniqueSemaphore acquireSemaphore;
	vk::UniqueFence renderFence;
	std::chrono::time_point<std::chrono::system_clock> lastStartTime;

	LifetimeExtender lifetime;

	VulkanDevice& device;
};

class FrameManager {
public:
	void init(VulkanDevice& device);
	void cleanup();

	void incrementFrame();
	float waitForCurrentFrameCompletion();
	void startCurrentFrame();

	std::array<std::shared_ptr<Frame>, FRAMES_IN_FLIGHT>& getFrames() { return frames; }

	inline uint32_t getCurrentFrameIndex() { return frameIndex; }
	inline std::shared_ptr<Frame> getCurrentFrame() { return frames[frameIndex]; }

private:
	std::array<std::shared_ptr<Frame>, FRAMES_IN_FLIGHT> frames;
	uint32_t frameNumber = 0;
	uint32_t frameIndex = 0;
};

#endif
