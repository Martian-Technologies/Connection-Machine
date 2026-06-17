#include "vulkanSwapchain.h"

void Swapchain::init(VulkanDevice& device, vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size) {
	this->device = &device;

	createSwapchain(surface, size, false);

	vk::SemaphoreCreateInfo semaphoreInfo{};
	imageSemaphores.resize(swapchain.image_count);
	for (uint32_t i = 0; i < swapchain.image_count; ++i) {
		auto result = device.getDevice().createSemaphoreUnique(semaphoreInfo);
		if (result.result != vk::Result::eSuccess) throwFatalError("failed to create swapchain semaphore");
		imageSemaphores[i] = std::move(result.value);
	}
}

void Swapchain::cleanup() {
	destroyFramebuffers();
	vkb::destroy_swapchain(swapchain);

	imageViews.clear();
	imageSemaphores.clear();
}

void Swapchain::recreate(vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size) {
	destroyFramebuffers();
	createSwapchain(surface, size, true);
}

const bool vsync = true;

void Swapchain::createSwapchain(vk::SurfaceKHR surface, std::pair<uint32_t, uint32_t> size, bool useOld) {
	vkb::SwapchainBuilder swapchainBuilder(device->getVkbDevice(), static_cast<VkSurfaceKHR>(surface));
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

	auto imageViewRet = swapchain.get_image_views();
	if (!imageViewRet) { throwFatalError("Could not get vulkan swapchain image views. Error: " + imageViewRet.error().message()); }
	imageViews = imageViewRet.value();
}

void Swapchain::destroyFramebuffers() {
	framebuffers.clear();
	swapchain.destroy_image_views(imageViews);
}

void Swapchain::createFramebuffers(vk::RenderPass renderPass, const AllocatedImage& colorImage) {
	framebuffers.resize(swapchain.image_count);

	for (size_t i = 0; i < swapchain.image_count; i++) {
		std::array<vk::ImageView, 2> attachments = {
			colorImage.imageView.get(),
			vk::ImageView(imageViews[i])
		};

		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchain.extent.width;
		framebufferInfo.height = swapchain.extent.height;
		framebufferInfo.layers = 1;

		auto fbResult = device->getDevice().createFramebufferUnique(framebufferInfo);
		if (fbResult.result != vk::Result::eSuccess) {
			throwFatalError("failed to create framebuffer!");
		}
		framebuffers[i] = std::move(fbResult.value);
	}
}
