#ifndef vulkanImageSwapchain_h
#define vulkanImageSwapchain_h

#include "gpu/vulkanDevice.h"
#include "gpu/abstractions/vulkanImage.h"

class ImageSwapchain {
public:
	struct Semaphore {
		Semaphore(vk::UniqueSemaphore semaphore) : semaphore(std::move(semaphore)) {}
		vk::UniqueSemaphore semaphore;
	};
	void init(VulkanDevice& device);
	void cleanup();

	void create(vk::RenderPass renderPass, std::pair<uint32_t, uint32_t> size, const AllocatedImage& colorImage, bool useOld);
	void recreate(vk::RenderPass renderPass, std::pair<uint32_t, uint32_t> size, const AllocatedImage& msaaImage);

	inline std::vector<std::shared_ptr<AllocatedImage>>& getImages() { return images; }
	inline std::vector<vk::UniqueFramebuffer>& getFramebuffers() { return framebuffers; }
	inline std::vector<std::shared_ptr<Semaphore>>& getImageSemaphores() { return imageSemaphores; }

private:
	std::vector<std::shared_ptr<AllocatedImage>> images;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<std::shared_ptr<Semaphore>> imageSemaphores;

	VulkanDevice* device = nullptr;
};

#endif /* vulkanImageSwapchain_h */
