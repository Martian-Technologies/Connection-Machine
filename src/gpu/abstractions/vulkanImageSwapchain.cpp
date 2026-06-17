#include "vulkanImageSwapchain.h"
#include "gpu/mainRenderer.h"
#include "gpu/renderer/frameManager.h"

void ImageSwapchain::init(VulkanDevice& device) {
	this->device = &device;

	vk::SemaphoreCreateInfo semaphoreInfo{};
	imageSemaphores.resize(FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < imageSemaphores.size(); ++i) {
		auto result = device.getDevice().createSemaphoreUnique(semaphoreInfo);
		if (result.result != vk::Result::eSuccess) throwFatalError("failed to create image semaphore");
		imageSemaphores[i] = std::make_shared<Semaphore>(std::move(result.value));
	}
}

void ImageSwapchain::cleanup() {
	framebuffers.clear();
	images.clear();
	imageSemaphores.clear();
}

const bool vsync = true;

void ImageSwapchain::create(vk::RenderPass renderPass, std::pair<uint32_t, uint32_t> size, const AllocatedImage& msaaImage, bool useOld) {
	images.resize(imageSemaphores.size());
	framebuffers.resize(images.size());
	for (unsigned int i = 0; i < images.size(); i++) {
		if (size.first == 0 || size.second == 0) return;

		vk::Extent3D imageSize = {size.first, size.second, 1};
		(void)device->getMaxUsableSampleCount();

		// Create resolve image (non-MSAA, for ImGui to sample)
		images[i] = std::make_shared<AllocatedImage>(
			*device,
			imageSize,
			vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			false,
			vk::SampleCountFlagBits::e1
		);

		// Create framebuffer with both attachments
		std::array<vk::ImageView, 2> attachments = {
			msaaImage.imageView.get(), // Color attachment (MSAA)
			images[i]->imageView.get() // Resolve attachment (non-MSAA)
		};

		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = size.first;
		framebufferInfo.height = size.second;
		framebufferInfo.layers = 1;

		auto fbResult = device->getDevice().createFramebufferUnique(framebufferInfo);
		if (fbResult.result != vk::Result::eSuccess) {
			throwFatalError("failed to create framebuffer!");
		}
		framebuffers[i] = std::move(fbResult.value);
	}
}

void ImageSwapchain::recreate(vk::RenderPass renderPass, std::pair<uint32_t, uint32_t> size, const AllocatedImage& msaaImage) {
	framebuffers.clear();
	images.clear();
	create(renderPass, size, msaaImage, true);
}
