#include "vulkanDevice.h"

#include "gpu/mainRenderer.h"
#include "gpu/renderer/viewport/blockTextureManager.h"

VulkanDevice::VulkanDevice(vk::SurfaceKHR surfaceForPresenting) {
	logInfo("Creating Vulkan Device...", "Vulkan");

	vkb::PhysicalDeviceSelector physicalDeviceSelector(MainRenderer::get().getVulkanInstance().getVkbInstance());
	physicalDeviceSelector.set_surface(static_cast<VkSurfaceKHR>(surfaceForPresenting));
	physicalDeviceSelector.add_required_extension("VK_KHR_push_descriptor");
	auto physicalDeviceRet = physicalDeviceSelector.select();
	if (!physicalDeviceRet) {
		throwFatalError("Could not select Vulkan physical device. Error: " + physicalDeviceRet.error().message());
	}
	physicalDevice = vk::PhysicalDevice(physicalDeviceRet.value().physical_device);

	vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();
	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & vk::SampleCountFlagBits::e64) msaaSamples = vk::SampleCountFlagBits::e64;
	else if (counts & vk::SampleCountFlagBits::e32) msaaSamples = vk::SampleCountFlagBits::e32;
	else if (counts & vk::SampleCountFlagBits::e16) msaaSamples = vk::SampleCountFlagBits::e16;
	else if (counts & vk::SampleCountFlagBits::e8) msaaSamples = vk::SampleCountFlagBits::e8;
	else if (counts & vk::SampleCountFlagBits::e4) msaaSamples = vk::SampleCountFlagBits::e4;
	else if (counts & vk::SampleCountFlagBits::e2) msaaSamples = vk::SampleCountFlagBits::e2;
	else msaaSamples = vk::SampleCountFlagBits::e1;

	vkb::DeviceBuilder deviceBuilder(physicalDeviceRet.value());
	auto deviceRet = deviceBuilder.build();
	if (!deviceRet) {
		throwFatalError("Could not create Vulkan device. Error: " + deviceRet.error().message());
	}
	vkbDevice = deviceRet.value();

	volkLoadDevice(vkbDevice);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device(vkbDevice.device));

	graphicsQueue = { vk::Queue(vkbDevice.get_queue(vkb::QueueType::graphics).value()), vkbDevice.get_queue_index(vkb::QueueType::graphics).value() };
	presentQueue = { vk::Queue(vkbDevice.get_queue(vkb::QueueType::present).value()), vkbDevice.get_queue_index(vkb::QueueType::present).value() };

	createAllocator();

	initializeImmediateSubmission();

	blockTextureManager.init(*this);
}

VulkanDevice::~VulkanDevice() {
	waitIdle();

	blockTextureManager.cleanup();
	immediateCommandPool.reset();
	immediateFence.reset();
	vmaAllocator.reset();

	vkb::destroy_device(vkbDevice);
}

void VulkanDevice::waitIdle() {
	std::lock_guard<std::mutex> lock(queueMux);
	(void)getDevice().waitIdle();
}

void VulkanDevice::waitIdleNoMux() {
	(void)getDevice().waitIdle();
}

vk::Result VulkanDevice::submitGraphicsQueue(const vk::SubmitInfo& submitInfo, vk::Fence fence) {
	std::lock_guard<std::mutex> lock(queueMux);
	return graphicsQueue.queue.submit(1, &submitInfo, fence);
}

vk::Result VulkanDevice::submitPresent(const vk::PresentInfoKHR& presentInfo) {
	std::lock_guard<std::mutex> lock(queueMux);
	return presentQueue.queue.presentKHR(&presentInfo);
}

void VulkanDevice::immediateSubmit(std::function<void(vk::CommandBuffer cmd)>&& function) {
	std::lock_guard<std::mutex> immediateLock(immediateSubmitMux);

	(void)getDevice().resetFences(immediateFence.get());
	immediateCommandBuffer.reset({});

	vk::CommandBufferBeginInfo cmdBeginInfo{};
	(void)immediateCommandBuffer.begin(cmdBeginInfo);

	function(immediateCommandBuffer);

	(void)immediateCommandBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &immediateCommandBuffer;
	{
		std::lock_guard<std::mutex> submitLock(queueMux);
		if (graphicsQueue.queue.submit(1, &submitInfo, immediateFence.get()) != vk::Result::eSuccess) {
			throwFatalError("failed to submit draw command buffer!");
		}
	}

	(void)getDevice().waitForFences(immediateFence.get(), VK_TRUE, UINT64_MAX);
}

void VulkanDevice::createAllocator() {
	vma::VulkanFunctions vulkanFunctions{};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;

	vma::AllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = getDevice();
	allocatorInfo.instance = MainRenderer::get().getVulkanInstance().getInstance();
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	auto allocResult = vma::createAllocatorUnique(allocatorInfo);
	if (allocResult.result != vk::Result::eSuccess) {
		throwFatalError("Could not create Vulkan VMA allocator.");
	}
	vmaAllocator = std::move(allocResult.value);
}

void VulkanDevice::initializeImmediateSubmission() {
	vk::CommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	commandPoolInfo.queueFamilyIndex = graphicsQueue.index;
	auto poolResult = getDevice().createCommandPoolUnique(commandPoolInfo);
	if (poolResult.result != vk::Result::eSuccess) throwFatalError("failed to create immediate command pool");
	immediateCommandPool = std::move(poolResult.value);

	vk::CommandBufferAllocateInfo commandBufferInfo{};
	commandBufferInfo.commandPool = immediateCommandPool.get();
	commandBufferInfo.commandBufferCount = 1;
	commandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
	auto cbResult = getDevice().allocateCommandBuffers(commandBufferInfo);
	if (cbResult.result != vk::Result::eSuccess) throwFatalError("failed to allocate immediate command buffer");
	immediateCommandBuffer = cbResult.value[0];

	vk::FenceCreateInfo fenceInfo{};
	auto fenceResult = getDevice().createFenceUnique(fenceInfo);
	if (fenceResult.result != vk::Result::eSuccess) throwFatalError("failed to create immediate fence");
	immediateFence = std::move(fenceResult.value);
}
