#ifndef sdlWindow_h
#define sdlWindow_h

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

class SdlWindow {
public:
	SdlWindow(const std::string& name);
	~SdlWindow();

	bool isThisMyEvent(const SDL_Event& event);
	
	VkSurfaceKHR createVkSurface(VkInstance instance);
	std::pair<uint32_t, uint32_t> getSize();

	inline float getWindowScalingSize() const { return windowScalingSize; }
	
	inline SDL_Window* getHandle() { return handle; }

	void toggleBorderlessFullscreen();

private:
	// TODO - smart pointer with custom deleter?
	SDL_Window* handle;

	float windowScalingSize;
	
	// Vulkan stuff
	VkInstance vkInstance;
	std::optional<VkSurfaceKHR> vkSurface;
};

#endif
