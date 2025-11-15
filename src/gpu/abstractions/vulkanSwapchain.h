#ifndef vulkanSwapchain_h
#define vulkanSwapchain_h

#include "gpu/vulkanDevice.h"

class Swapchain {
public:
	void init(VulkanDevice* device, VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size);
	void cleanup();

	void createFramebuffers(VkRenderPass renderPass, const AllocatedImage& colorImage);
	void recreate(VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size);

	inline vkb::Swapchain& getSwapchain() { return swapchain; }
	inline std::vector<VkFramebuffer>& getFramebuffers() { return framebuffers; }
	inline std::vector<VkSemaphore>& getImageSemaphores() { return imageSemaphores; }

private:
	void createSwapchain(VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size, bool useOld);
	void destroyFramebuffers();

private:
	vkb::Swapchain swapchain;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkImageView> imageViews;
	std::vector<VkSemaphore> imageSemaphores;

	VulkanDevice* device;
};

#endif
