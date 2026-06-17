#include "windowRenderer.h"
#include "app.h"
#include "imgui/imgui_impl_vulkan.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "gpu/mainRenderer.h"

WindowRenderer::WindowRenderer(WindowId windowId, SdlWindow* sdlWindow) : windowId(windowId), sdlWindow(sdlWindow) {
	logInfo("Initializing window renderer...");

	VkSurfaceKHR rawSurface = sdlWindow->createVkSurface(MainRenderer::get().getVulkanInstance().getVkbInstance());
	surface = vk::SurfaceKHR(rawSurface);
	device = &MainRenderer::get().getVulkanInstance().createOrGetDevice(surface);

	frames.init(*device);

	std::pair<uint32_t, uint32_t> sdlWindowSize = sdlWindow->getSize();
	windowSize ={max(sdlWindowSize.first, 1), max(sdlWindowSize.second, 1)};
	swapchain.init(*device, surface, windowSize);
	createRenderPass();
	createColorResources();
	swapchain.createFramebuffers(renderPass.get(), *colorImage);

	imGuiRenderer.emplace(sdlWindow->getHandle(), renderPass.get(), FRAMES_IN_FLIGHT);

	running.store(true);
	renderLoopStopped.store(false);
	renderThread = std::thread(&WindowRenderer::renderLoop, this);
}

WindowRenderer::~WindowRenderer() {
	// stop render thread (not completely sure if this is right for the destructor yet)
	running.store(false);
	while (!renderLoopStopped.load()) App::doRunOnMainForThread(renderThread.get_id());
	if (renderThread.joinable()) renderThread.join();

	device->waitIdle();
	renderPass.reset();
	swapchain.cleanup();
	frames.cleanup();
}

void WindowRenderer::resize(std::pair<uint32_t, uint32_t> windowSize) {
	std::lock_guard<std::mutex> lock(windowSizeMux);

	this->windowSize = {max(windowSize.first, 1), max(windowSize.second, 1)};

	swapchainRecreationNeeded = true;
}

void WindowRenderer::renderLoop() {
	while(running.load()) {
		std::shared_ptr<Frame> frame = frames.getCurrentFrame();

		frames.waitForCurrentFrameCompletion();

		if (swapchainRecreationNeeded) {
			recreateSwapchain();
			continue;
		}

		// try to start rendering the frame
		// get next swapchain image to render to (or fail and try again)
		uint32_t imageIndex = 0;
		vk::Result acquireResult = static_cast<vk::Result>(vkAcquireNextImageKHR(
			static_cast<VkDevice>(device->getDevice()),
			static_cast<VkSwapchainKHR>(swapchain.getSwapchainHandle()),
			UINT64_MAX,
			static_cast<VkSemaphore>(frame->acquireSemaphore.get()),
			VK_NULL_HANDLE,
			&imageIndex));
		if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR) {
			swapchainRecreationNeeded = true;
			if (acquireResult == vk::Result::eErrorOutOfDateKHR) continue;
		} else if (acquireResult != vk::Result::eSuccess) {
			logError("failed to acquire swap chain image!");
			continue;
		}

		frames.startCurrentFrame();

		{
			std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
			frame->mainCommandBuffer.reset({});
		}

		imGuiRenderer->beginFrame();
		semaphoreForThisFrame.clear();
		MainRenderer::get().setCurrentlyRenderedWindow(windowId);
		sdlWindow->doRendering();
		MainRenderer::get().clearCurrentlyRenderedWindow();
		renderToCommandBuffer(*frame, imageIndex);

		vk::SubmitInfo submitInfo{};

		semaphoreForThisFrame.push_back(frame->acquireSemaphore.get());
		std::vector<vk::PipelineStageFlags> waitStages(semaphoreForThisFrame.size(), vk::PipelineStageFlagBits::eColorAttachmentOutput);
		submitInfo.waitSemaphoreCount = semaphoreForThisFrame.size();
		submitInfo.pWaitSemaphores = semaphoreForThisFrame.data();
		submitInfo.pWaitDstStageMask = waitStages.data();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame->mainCommandBuffer;

		vk::Semaphore signalSemaphores[] = { swapchain.getImageSemaphores()[imageIndex].get() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (device->submitGraphicsQueue(submitInfo, frame->renderFence.get()) != vk::Result::eSuccess){
			logError("failed to submit draw command buffer!");
		}

		vk::PresentInfoKHR presentInfo{};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		vk::SwapchainKHR swapchains[] = { swapchain.getSwapchainHandle() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		vk::Result imagePresentResult = device->submitPresent(presentInfo);
		if (imagePresentResult == vk::Result::eErrorOutOfDateKHR || imagePresentResult == vk::Result::eSuboptimalKHR) {
			swapchainRecreationNeeded = true;
		} else if (imagePresentResult != vk::Result::eSuccess) {
			logError("failed to present swap chain image!");
		}

		frames.incrementFrame();
#ifdef TRACY_PROFILER
		FrameMark;
#endif
	}
	renderLoopStopped.store(true);
	renderLoopStopped.notify_all();
}

void WindowRenderer::renderToCommandBuffer(Frame& frame, uint32_t imageIndex) {
	vk::Extent2D windowSize = vk::Extent2D(swapchain.getSwapchain().extent.width, swapchain.getSwapchain().extent.height);

	vk::CommandBufferBeginInfo beginInfo{};
	if (frame.mainCommandBuffer.begin(beginInfo) != vk::Result::eSuccess) {
		throwFatalError("failed to begin recording command buffer!");
	}

	vk::ClearValue clearValues[1];
	clearValues[0].color = vk::ClearColorValue(std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f });

	vk::RenderPassBeginInfo renderPassInfo{};
	renderPassInfo.renderPass = renderPass.get();
	renderPassInfo.framebuffer = swapchain.getFramebuffers()[imageIndex].get();
	renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
	renderPassInfo.renderArea.extent = windowSize;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = clearValues;

	frame.mainCommandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	{
		imGuiRenderer->endFrame(frame.mainCommandBuffer);
	}

	frame.mainCommandBuffer.endRenderPass();
	if (frame.mainCommandBuffer.end() != vk::Result::eSuccess) {
		throwFatalError("failed to record command buffer!");
	}
}

void WindowRenderer::createRenderPass() {
	vk::SampleCountFlagBits msaaSamples = device->getMaxUsableSampleCount();

	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = vk::Format(swapchain.getSwapchain().image_format);
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = vk::Format(swapchain.getSwapchain().image_format);
	colorAttachmentResolve.samples = vk::SampleCountFlagBits::e1;
	colorAttachmentResolve.loadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachmentResolve.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachmentResolve.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachmentResolve.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachmentResolve.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachmentResolve.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference colorAttachmentResolveRef{};
	colorAttachmentResolveRef.attachment = 1;
	colorAttachmentResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	vk::SubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = {};
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

	std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, colorAttachmentResolve};
	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	auto result = device->getDevice().createRenderPassUnique(renderPassInfo);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create MSAA render pass!");
	}
	renderPass = std::move(result.value);
}

void WindowRenderer::createColorResources() {
	vk::Extent3D size = {swapchain.getSwapchain().extent.width, swapchain.getSwapchain().extent.height, 1};
	vk::Format colorFormat = vk::Format(swapchain.getSwapchain().image_format);
	vk::SampleCountFlagBits msaaSamples = device->getMaxUsableSampleCount();

	colorImage.emplace(
		*device,
		size,
		colorFormat,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		false,
		msaaSamples
	);
}

void WindowRenderer::recreateSwapchain() {
	std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
	device->waitIdleNoMux();

	std::lock_guard<std::mutex> lock(windowSizeMux);

	colorImage.reset();
	swapchain.recreate(surface, windowSize);
	createColorResources();
	swapchain.createFramebuffers(renderPass.get(), *colorImage);

	swapchainRecreationNeeded = false;
}

void WindowRenderer::setImGuiRenderFunc(std::function<void()> imGuiRenderFunc) {
	std::lock_guard<std::mutex> lock(imGuiRenderFuncMux);
	this->imGuiRenderFunc = imGuiRenderFunc;
}

void WindowRenderer::updateImGuiBlockTextureArrayLayers() {
	std::lock_guard lock(imGuiBlockTextureArrayLayersMux);
	if (blockTextureArrayImage == nullptr) {
		assert(imGuiBlockTextureArrayLayers.empty());
		blockTextureArrayImage = MainRenderer::get().getVulkanInstance().getDevice().getBlockTextureManager().getTextureArray();
	} else {
		std::shared_ptr<BlockTextureArray> image = MainRenderer::get().getVulkanInstance().getDevice().getBlockTextureManager().getTextureArray();
		if (blockTextureArrayImage == image) return; // image has not changed
		imGuiBlockTextureArrayLayers.clear();
		blockTextureArrayImage = image;
	}
	if (blockTextureArrayImage == nullptr) return;
	if (!blockTextureArrayImage->image.has_value()) return;
	for (unsigned int i = 0; i < blockTextureArrayImage->image->arrayLayers; i++) {
		imGuiBlockTextureArrayLayers.emplace_back(std::make_shared<ImGuiRenderer::ImGuiDescriptorSet>(
			vk::DescriptorSet(ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(blockTextureArrayImage->sampler.get()), static_cast<VkImageView>(blockTextureArrayImage->image->layerViews[i].get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)),
			imGuiRenderer.value(),
			std::vector<std::shared_ptr<void>>{ blockTextureArrayImage }
		));
	}
}
