#include "vulkanInstance.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

VulkanInstance::VulkanInstance() {
	logInfo("Initializing Vulkan...", "Vulkan");

	// Initialize volk loader
	if (volkInitialize() != VK_SUCCESS) {
		throwFatalError("Failed to find Vulkan loader, Connection Machine requires Vulkan. Please make sure that your graphics drivers are installed and up to date.");
	}

	// Start creating vulkan instance
	vkb::InstanceBuilder instanceBuilder(vkGetInstanceProcAddr);

	// Get Vulkan system information
	auto systemInfoRet = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
	if (!systemInfoRet) { throwFatalError("Could not fetch Vulkan system info. Error: " + systemInfoRet.error().message()); }
	auto systemInfo = systemInfoRet.value();

#ifndef NDEBUG
	#if not __APPLE__
		// Enable validation layers
		if (systemInfo.validation_layers_available){
			instanceBuilder.enable_validation_layers().set_debug_callback(&vulkanDebugCallback);
		}
	#endif
#endif

	// Create Vulkan Instance
	instanceBuilder.set_app_name("Connection Machine");
	instanceBuilder.set_engine_name("Jack Jamison's Wacky-n-Wonderful Connection Machine Render-a-tron 3000 million!");
	instanceBuilder.require_api_version(1,0,0);
	auto instanceRet = instanceBuilder.build();
	if (!instanceRet) { throwFatalError("Failed to create Vulkan instance. Error: " + instanceRet.error().message()); }
	instance = instanceRet.value();

	// Load instance functions
	volkLoadInstance(instance);
}

VulkanInstance::~VulkanInstance() {
	logInfo("Shutting down Vulkan..", "Vulkan");

	device.reset();
	vkb::destroy_instance(instance);
}

VulkanDevice* VulkanInstance::createOrGetDevice(VkSurfaceKHR surfaceForPresenting) {
	// create device if one doesn't exist
	if (!device.has_value()) {
		device.emplace(surfaceForPresenting);
	}

	return &device.value();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		logInfo(pCallbackData->pMessage, "VK Validation");
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		logWarning(pCallbackData->pMessage, "VK Validation");
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		logError(pCallbackData->pMessage, "VK Validation");
		break;
	default:
		break;
	}

    return VK_FALSE;
}
