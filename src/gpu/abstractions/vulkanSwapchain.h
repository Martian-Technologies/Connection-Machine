#ifndef vulkanSwapchain_h
#define vulkanSwapchain_h

#include "gpu/vulkanDevice.h"
#include "gpu/abstractions/vulkanImage.h"

class Swapchain {
public:
	void init(VulkanDevice& device, vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size);
	void cleanup();

	void createFramebuffers(vk::RenderPass renderPass, const AllocatedImage& colorImage);
	void recreate(vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size);

	inline vkb::Swapchain& getSwapchain() { return swapchain; }
	inline vk::SwapchainKHR getSwapchainHandle() { return vk::SwapchainKHR(swapchain.swapchain); }
	inline std::vector<vk::UniqueFramebuffer>& getFramebuffers() { return framebuffers; }
	inline std::vector<vk::UniqueSemaphore>& getImageSemaphores() { return imageSemaphores; }

private:
	void createSwapchain(vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size, bool useOld);
	void destroyFramebuffers();

private:
	vkb::Swapchain swapchain;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<VkImageView> imageViews;
	std::vector<vk::UniqueSemaphore> imageSemaphores;

	VulkanDevice* device = nullptr;
};

#endif /* vulkanSwapchain_h */
