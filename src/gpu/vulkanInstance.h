#ifndef vulkanInstance_h
#define vulkanInstance_h

#include <volk.h>
#include <VkBootstrap.h>

#include "vulkanDevice.h"

class VulkanInstance {
public:
	VulkanInstance();
	~VulkanInstance();

	VulkanDevice* createOrGetDevice(VkSurfaceKHR surfaceForPresenting);

	inline vkb::Instance getVkbInstance() { return instance; };

private:
	vkb::Instance instance;
	std::optional<VulkanDevice> device;
};

// VULKAN TODO -
// - [x] Vulkan chunker with wires - Chunk system should be abstracted somewaht (just also supports wires, move a few functions out)
// - [x] Better naming convention for classes
// - [x] Block textures
// - [x] Wires
// - [x] State
// - [x] Effects (elements)

// VULKAN IMPROVEMENTS -
// - [x] VkBootstrap
// - [x] Switch from singleton to top down design
// - [x] Volk dynamic loader
// - [x] Standardization of subrenderer params and input, better way for subrenderer to communicate and put data on the "frame", growable descriptor pool
// - [x] Fix validation layers on mac, and weird resize messages on x11
// - [x] Use dynamic rendering, push descriptors and other QOL extensions to simplify code
// - [ ] Vertex pulling
// - [ ] Staging Buffers
// - [ ] Pooled async resource uploading
// - [ ] Check macro
// - [ ] Don't draw directly to swapchain

// POSSIBLE SETTINGS -
// Vsync
// Frame limit
// Mip mapping
// Anti aliasing
// Chunk size?

#endif
