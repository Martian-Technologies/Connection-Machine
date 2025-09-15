#include "vulkanDevice.h"

#include "gpu/renderer/viewport/blockTextureManager.h"
#include "gpu/mainRenderer.h"

VulkanDevice::VulkanDevice(VkSurfaceKHR surfaceForPresenting) {
	logInfo("Creating Vulkan Device...", "Vulkan");

	// Select physical device
	vkb::PhysicalDeviceSelector physicalDeviceSelector(MainRenderer::get().getVulkanInstance().getVkbInstance());
	physicalDeviceSelector.set_surface(surfaceForPresenting);
	physicalDeviceSelector.add_required_extension("VK_KHR_push_descriptor");
	auto physicalDeviceRet = physicalDeviceSelector.select();
	if (!physicalDeviceRet) { throwFatalError("Could not select Vulkan physical device. Error: " + physicalDeviceRet.error().message()); }
	physicalDevice = physicalDeviceRet.value().physical_device;

	// Build device
	vkb::DeviceBuilder deviceBuilder(physicalDeviceRet.value());
	auto deviceRet = deviceBuilder.build();
	if (!deviceRet) { throwFatalError("Could not create Vulkan device. Error: " + deviceRet.error().message()); }
	device = deviceRet.value();

	// Load device functions
	volkLoadDevice(device);

	// Get queues
	graphicsQueue = { device.get_queue(vkb::QueueType::graphics).value(),
		device.get_queue_index(vkb::QueueType::graphics).value() };
	presentQueue = { device.get_queue(vkb::QueueType::present).value(),
		device.get_queue_index(vkb::QueueType::present).value() };

	// Create vma allocator
	createAllocator();

	// Initialize immediate submission
	initializeImmediateSubmission();

	// Set up texture manager
	blockTextureManager.init(this);

	// Set up rml resource manager
	rmlResourceManager.init(this);
}

VulkanDevice::~VulkanDevice() {
	waitIdle();

	rmlResourceManager.cleanup();
	blockTextureManager.cleanup();
	vmaDestroyAllocator(vmaAllocator);

	vkDestroyFence(device, immediateFence, nullptr);
	vkDestroyCommandPool(device, immediateCommandPool, nullptr);
	vkb::destroy_device(device);
}

void VulkanDevice::waitIdle() {
	std::lock_guard<std::mutex> lock(queueMux);
	vkDeviceWaitIdle(device);
}

VkResult VulkanDevice::submitGraphicsQueue(VkSubmitInfo* submitInfo, VkFence fence) {
	std::lock_guard<std::mutex> lock(queueMux);
	return vkQueueSubmit(graphicsQueue.queue, 1, submitInfo, fence);
}

VkResult VulkanDevice::submitPresent(VkPresentInfoKHR* presentInfo) {
	std::lock_guard<std::mutex> lock(queueMux);
	return vkQueuePresentKHR(presentQueue.queue, presentInfo);
}

void VulkanDevice::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
	std::lock_guard<std::mutex> immediateLock(immediateSubmitMux);

	// set up
	vkResetFences(device, 1, &immediateFence);
	vkResetCommandBuffer(immediateCommandBuffer, 0);

	// start command buffer
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(immediateCommandBuffer, &cmdBeginInfo);

	// run commands
	function(immediateCommandBuffer);

	// end command buffer
	vkEndCommandBuffer(immediateCommandBuffer);

	// submit command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &immediateCommandBuffer;
	{
		std::lock_guard<std::mutex> submitLock(queueMux);
		if (vkQueueSubmit(graphicsQueue.queue, 1, &submitInfo, immediateFence) != VK_SUCCESS) {
			throwFatalError("failed to submit draw command buffer!");
		}
	}

	// wait for completion
	vkWaitForFences(device, 1, &immediateFence, VK_TRUE, UINT64_MAX);
}

void VulkanDevice::createAllocator() {
	// function pointers from volk for VMA to use
	const auto vulkanFunctions = VmaVulkanFunctions{
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
	};

	VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = MainRenderer::get().getVulkanInstance().getVkbInstance();
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	VmaAllocator alloc;
    if (vmaCreateAllocator(&allocatorInfo, &alloc) != VK_SUCCESS) { throwFatalError("Could not create Vulkan VMA allocator.");}
	vmaAllocator = alloc;
}

void VulkanDevice::initializeImmediateSubmission() {
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = graphicsQueue.index;
	vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immediateCommandPool);

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.pNext = nullptr;
	commandBufferInfo.commandPool = immediateCommandPool;
	commandBufferInfo.commandBufferCount = 1;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkAllocateCommandBuffers(device, &commandBufferInfo, &immediateCommandBuffer);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	vkCreateFence(device, &fenceInfo, nullptr, &immediateFence);
}
