#include "frameManager.h"

#include "gpu/vulkanDevice.h"

Frame::Frame(VulkanDevice& device) : device(device) {
	vk::CommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	commandPoolInfo.queueFamilyIndex = device.getGraphicsQueueIndex();
	auto poolResult = device.getDevice().createCommandPoolUnique(commandPoolInfo);
	if (poolResult.result != vk::Result::eSuccess) throwFatalError("failed to create frame command pool");
	commandPool = std::move(poolResult.value);

	vk::CommandBufferAllocateInfo commandBufferInfo{};
	commandBufferInfo.commandPool = commandPool.get();
	commandBufferInfo.commandBufferCount = 1;
	commandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
	auto cbResult = device.getDevice().allocateCommandBuffers(commandBufferInfo);
	if (cbResult.result != vk::Result::eSuccess) throwFatalError("failed to allocate frame command buffer");
	mainCommandBuffer = cbResult.value[0];

	vk::FenceCreateInfo fenceInfo{};
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
	auto fenceResult = device.getDevice().createFenceUnique(fenceInfo);
	if (fenceResult.result != vk::Result::eSuccess) throwFatalError("failed to create frame fence");
	renderFence = std::move(fenceResult.value);

	vk::SemaphoreCreateInfo semaphoreInfo{};
	auto semResult = device.getDevice().createSemaphoreUnique(semaphoreInfo);
	if (semResult.result != vk::Result::eSuccess) throwFatalError("failed to create frame semaphore");
	acquireSemaphore = std::move(semResult.value);
}

void FrameManager::init(VulkanDevice& device) {
	for (std::shared_ptr<Frame>& frame : frames) {
		frame = std::make_shared<Frame>(device);
	}
}

void FrameManager::cleanup() {
	for (std::shared_ptr<Frame>& frame : frames) {
		frame = nullptr;
	}
}

void FrameManager::incrementFrame() {
	++frameNumber;
	frameIndex = frameNumber % frames.size();
}

float FrameManager::waitForCurrentFrameCompletion() {
	(void)frames[frameIndex]->device.getDevice().waitForFences(frames[frameIndex]->renderFence.get(), VK_TRUE, UINT64_MAX);

	auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - frames[frameIndex]->lastStartTime).count() / 1000.0f;

	frames[frameIndex]->lifetime.flush();

	return time;
}

void FrameManager::startCurrentFrame() {
	(void)frames[frameIndex]->device.getDevice().resetFences(frames[frameIndex]->renderFence.get());

	frames[frameIndex]->lastStartTime = std::chrono::system_clock::now();
}
