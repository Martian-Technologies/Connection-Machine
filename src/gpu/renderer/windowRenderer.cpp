#include "windowRenderer.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "gpu/mainRenderer.h"

WindowRenderer::WindowRenderer(SdlWindow* sdlWindow) : sdlWindow(sdlWindow) {
	logInfo("Initializing window renderer...");

	// create surface and use it to make sure a vulkan device has been created
	surface = sdlWindow->createVkSurface(MainRenderer::get().getVulkanInstance().getVkbInstance());
	device = MainRenderer::get().getVulkanInstance().createOrGetDevice(surface);

	// initialize frames
	frames.init(device);

	// set up swapchain and subrenderer
	std::pair<uint32_t, uint32_t> sdlWindowSize = sdlWindow->getSize();
	windowSize ={max(sdlWindowSize.first, 1), max(sdlWindowSize.second, 1)}; // should stop vulkan validation error
	swapchain.init(device, surface, windowSize);
	createRenderPass();
	createColorResources();
	swapchain.createFramebuffers(renderPass, colorImage);

	// subrenderers
	rmlRenderer.init(device, renderPass);
	viewportRenderer.init(device, renderPass);

	// start render loop
	running = true;
	renderThread = std::thread(&WindowRenderer::renderLoop, this);
}

WindowRenderer::~WindowRenderer() {
	// stop render thread (not completely sure if this is right for the destructor yet)
	running = false;
	if (renderThread.joinable()) renderThread.join();

	// start by deleting render pass
	vkDestroyRenderPass(device->getDevice(), renderPass, nullptr);
	// now the swapchain can be deleted
	swapchain.cleanup();
	// now the frames are free!
	frames.cleanup();
	// clean up color image for multisampled
	destroyImage(colorImage);

	// delete rml renderer
	rmlRenderer.cleanup();
	// delete viewport renderer
	viewportRenderer.cleanup();
}

void WindowRenderer::resize(std::pair<uint32_t, uint32_t> windowSize) {
	std::lock_guard<std::mutex> lock(windowSizeMux);

	this->windowSize = {max(windowSize.first, 1), max(windowSize.second, 1)}; // should stop vulkan validation error

	swapchainRecreationNeeded = true;
}

void WindowRenderer::renderLoop() {
	while(running) {
		Frame& frame = frames.getCurrentFrame();

		// wait for frame completion
		frames.waitForCurrentFrameCompletion();

		// recreate swapchain if needed
		if (swapchainRecreationNeeded) {
			recreateSwapchain();
			continue;
		}

		// try to start rendering the frame
		// get next swapchain image to render to (or fail and try again)
		uint32_t imageIndex;
		VkResult imageGetResult = vkAcquireNextImageKHR(device->getDevice(), swapchain.getSwapchain(), UINT64_MAX, frame.acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (imageGetResult == VK_ERROR_OUT_OF_DATE_KHR || imageGetResult == VK_SUBOPTIMAL_KHR) {
			// if the swapchain is not ideal, try again but recreate it this time (this happens in normal operation)
			swapchainRecreationNeeded = true;

			if (imageGetResult == VK_ERROR_OUT_OF_DATE_KHR) continue;
		} else if (imageGetResult != VK_SUCCESS) {
			// if the error was even worse (one could say exceptional), we log an error and pray
			logError("failed to acquire swap chain image!");
			continue;
		}

		// tell frame that it has started
		frames.startCurrentFrame();

		// record command buffer
		vkResetCommandBuffer(frame.mainCommandBuffer, 0);
		renderToCommandBuffer(frame, imageIndex);

		// start setting up graphics submission ====================================================
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// wait semaphore
		VkSemaphore waitSemaphores[] = { frame.acquireSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		// command buffers
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame.mainCommandBuffer;

		// signal semaphores
		VkSemaphore signalSemaphores[] = { swapchain.getImageSemaphores()[imageIndex] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// submit to queue
		if (device->submitGraphicsQueue(&submitInfo, frame.renderFence) != VK_SUCCESS){
			logError("failed to submit draw command buffer!");
		}

		// start setting up present submission =====================================================
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapchains[] = { swapchain.getSwapchain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // unused

		// submit to present queue
		VkResult imagePresentResult = device->submitPresent(&presentInfo);
		if (imagePresentResult == VK_ERROR_OUT_OF_DATE_KHR || imagePresentResult == VK_SUBOPTIMAL_KHR) {
			swapchainRecreationNeeded = true;
		} else if (imagePresentResult != VK_SUCCESS) {
			logError("failed to present swap chain image!");
		}

		//increase the number of frames drawn
		frames.incrementFrame();
#ifdef TRACY_PROFILER
		FrameMark;
#endif
	}

	device->waitIdle();
}

void WindowRenderer::renderToCommandBuffer(Frame& frame, uint32_t imageIndex) {
	// preparation
	VkExtent2D windowSize = swapchain.getSwapchain().extent;

	// start recording
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional
	if (vkBeginCommandBuffer(frame.mainCommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkClearValue clearValues[1];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	// begin render pass
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapchain.getFramebuffers()[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapchain.getSwapchain().extent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(frame.mainCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// do actual rendering...
	{
		// viewports
		std::lock_guard<std::mutex> lock(viewportRenderersMux);
		for (ViewportRenderData* viewport : viewportRenderDatas) {
			viewportRenderer.render(frame, viewport);
		}

		// rml rendering
		rmlRenderer.render(frame, windowSize);
	}

	// end render pass
	vkCmdEndRenderPass(frame.mainCommandBuffer);
	if (vkEndCommandBuffer(frame.mainCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void WindowRenderer::createRenderPass() {
	VkSampleCountFlagBits msaaSamples = device->getMaxUsableSampleCount();

    // Multisampled color attachment (offscreen)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getSwapchain().image_format;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Resolve attachment (the swapchain image)
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapchain.getSwapchain().image_format;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // References
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 1;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    // Subpass dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create render pass
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, colorAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create MSAA render pass!");
    }
}

void WindowRenderer::createColorResources() {
    VkExtent3D size = {swapchain.getSwapchain().extent.width, swapchain.getSwapchain().extent.height, 1};
    VkFormat colorFormat = swapchain.getSwapchain().image_format;
    VkSampleCountFlagBits msaaSamples = device->getMaxUsableSampleCount();

    colorImage = createImage(
        device,
        size,
        colorFormat,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        false,
        msaaSamples
    );
}

void WindowRenderer::recreateSwapchain() {
	device->waitIdle();

	std::lock_guard<std::mutex> lock(windowSizeMux);

	destroyImage(colorImage);
	swapchain.recreate(surface, windowSize);
	createColorResources();
	swapchain.createFramebuffers(renderPass, colorImage);

	swapchainRecreationNeeded = false;
}

void WindowRenderer::registerViewportRenderData(ViewportRenderData *viewportRenderData) {
	std::lock_guard<std::mutex> lock(viewportRenderersMux);
	viewportRenderDatas.insert(viewportRenderData);
}

void WindowRenderer::deregisterViewportRenderData(ViewportRenderData* viewportRenderData) {
	std::lock_guard<std::mutex> lock(viewportRenderersMux);
	viewportRenderDatas.erase(viewportRenderData);
}

bool WindowRenderer::hasViewportRenderData(ViewportRenderData* viewportRenderData) {
	std::lock_guard<std::mutex> lock(viewportRenderersMux);
	return viewportRenderDatas.contains(viewportRenderData);
}
