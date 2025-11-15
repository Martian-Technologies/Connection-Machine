#include "vulkanSwapchain.h"

void Swapchain::init(VulkanDevice* device, VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size) {
	this->device = device;

	createSwapchain(surface, size, false);

	// create image semaphores
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	imageSemaphores.resize(swapchain.image_count);
	for (uint32_t i = 0; i < swapchain.image_count; ++i) {
		vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &imageSemaphores[i]);
	}
}

void Swapchain::cleanup() {
	destroyFramebuffers();
	vkb::destroy_swapchain(swapchain);

	imageViews.clear();

	for (uint32_t i = 0; i < swapchain.image_count; ++i) {
		vkDestroySemaphore(device->getDevice(), imageSemaphores[i], nullptr);
	}
}

void Swapchain::recreate(VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size) {
	destroyFramebuffers();
	createSwapchain(surface, size, true);
}

const bool vsync = true;

void Swapchain::createSwapchain(VkSurfaceKHR surface, std::pair<uint32_t, uint32_t> size, bool useOld) {
	// Create swapchain
	vkb::SwapchainBuilder swapchainBuilder(device->getDevice(), surface);
	swapchainBuilder.set_desired_extent(size.first, size.second);
	if (vsync) swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);
	else swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
	swapchainBuilder.set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
	if (useOld) swapchainBuilder.set_old_swapchain(swapchain);

	auto swapchainRet = swapchainBuilder.build();
	if (!swapchainRet)
		throwFatalError("Could not create vulkan swapchain. Error: " + swapchainRet.error().message());
	if (useOld)
		vkb::destroy_swapchain(swapchain);
	swapchain = swapchainRet.value();

	// Get image views
	auto imageViewRet = swapchain.get_image_views();
	if (!imageViewRet) { throwFatalError("Could not get vulkan swapchain image views. Error: " + imageViewRet.error().message()); }
	imageViews = imageViewRet.value();
}

// Destroy Frame buffers and image views
void Swapchain::destroyFramebuffers() {
	for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(device->getDevice(), framebuffer, nullptr);
    }
	framebuffers.clear();
	swapchain.destroy_image_views(imageViews);
}

// Create and initialize framebuffers for use with the swapchain
void Swapchain::createFramebuffers(const VkRenderPass renderPass, const AllocatedImage& colorImage) {
	framebuffers.resize(swapchain.image_count);

	for (size_t i = 0; i < swapchain.image_count; i++) {
		std::array<VkImageView, 2> attachments = {
            colorImage.imageView,   // multisampled color buffer
            imageViews[i]      // resolve (swapchain) image
        };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchain.extent.width;
		framebufferInfo.height = swapchain.extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device->getDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

