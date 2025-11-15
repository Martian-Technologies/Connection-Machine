#include "vulkanInstance.h"
#include "gui/sdl/sdlWindow.h"

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

	logInfo("Loader initialized", "Vulkan");

	// Start creating vulkan instance
	vkb::InstanceBuilder instanceBuilder(vkGetInstanceProcAddr);

	// Get Vulkan system information
	auto systemInfoRet = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
	if (!systemInfoRet) { throwFatalError("Could not fetch Vulkan system info. Error: " + systemInfoRet.error().message()); }
	auto systemInfo = systemInfoRet.value();

	logInfo("API Version Supported by Instance: " +
	        std::to_string(VK_VERSION_MAJOR(systemInfo.instance_api_version)) + "." +
	        std::to_string(VK_VERSION_MINOR(systemInfo.instance_api_version)) + "." +
	        std::to_string(VK_VERSION_PATCH(systemInfo.instance_api_version)),
		"Vulkan");
	logInfo("Validation Layers Available: " + std::string(systemInfo.validation_layers_available ? "Yes" : "No"), "Vulkan");
	logInfo("Debug Utils Available: " + std::string(systemInfo.debug_utils_available ? "Yes" : "No"), "Vulkan");

#ifndef NDEBUG
	// Enable validation layers
	if (systemInfo.validation_layers_available){
		instanceBuilder.enable_validation_layers().set_debug_callback(&vulkanDebugCallback);
		logInfo("Enabled validation layers", "Vulkan");
	}
#endif

	// Create Vulkan Instance
	instanceBuilder.set_app_name("Connection Machine");
	instanceBuilder.set_engine_name("Jack Jamison's Wacky-n-Wonderful Connection Machine Render-a-tron 3000 million!");
	instanceBuilder.require_api_version(1,0,0);
	auto instanceRet = instanceBuilder.build();
	if (!instanceRet) { throwFatalError("Failed to create Vulkan instance. Error: " + instanceRet.error().message()); }
	instance = instanceRet.value();

	logInfo("Vulkan instance created successfully", "Vulkan");

	// Load instance functions
	volkLoadInstance(instance);

	logInfo("Vulkan initialized successfully", "Vulkan");
}

VulkanInstance::~VulkanInstance() {
	logInfo("Shutting down Vulkan..", "Vulkan");

	device.reset();
	vkb::destroy_instance(instance);
}

VulkanDevice* VulkanInstance::getDevice() {
	if (!device.has_value()) {
		logInfo("Creating tmp SDL window to create devise");
		SdlWindow sdlWindow("tmp", 1, 1);
		VkSurfaceKHR surface = sdlWindow.createVkSurface(getVkbInstance());
		device.emplace(surface);
	}
	return &device.value();
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
    void* pUserData
) {
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
