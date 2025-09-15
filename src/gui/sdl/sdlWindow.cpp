#include "sdlWindow.h"
#include "util/fastMath.h"

SdlWindow::SdlWindow(const std::string& name) {
	logInfo("Creating SDL window...");

	handle = SDL_CreateWindow(name.c_str(), 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!handle)
	{
		throwFatalError("SDL could not create window! SDL_Error: " + std::string(SDL_GetError()));
	}

	int winW, winH, drawW, drawH;
	SDL_GetWindowSize(handle, &winW, &winH);
	SDL_GetWindowSizeInPixels(handle, &drawW, &drawH);

	float scaleX = (float)drawW / winW;
	float scaleY = (float)drawH / winH;

	if (!approx_equals(scaleX, scaleY)) {
		logError("width scale not the same x={} y={}", "SdlWindow", scaleX, scaleY);
	}

	windowScalingSize = scaleX;
}

SdlWindow::~SdlWindow() {
	logInfo("Destroying SDL window...");

	if (vkSurface.has_value()) SDL_Vulkan_DestroySurface(vkInstance, vkSurface.value(), nullptr);
	SDL_DestroyWindow(handle);
}

bool SdlWindow::isThisMyEvent(const SDL_Event& event) {
	if (event.type == 2050) return true; // the fuck is this? - jack quay jamison
	return SDL_GetWindowFromEvent(&event) == handle;
}

std::pair<uint32_t, uint32_t> SdlWindow::getSize() {
	int w, h;
	SDL_GetWindowSize(handle, &w, &h);
	return { w, h };
}

VkSurfaceKHR SdlWindow::createVkSurface(VkInstance instance) {
	if (vkSurface.has_value()) return vkSurface.value();

	vkInstance = instance;

	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(handle, instance, nullptr, &surface)) {
		throwFatalError("SDL could not create vulkan surface! SDL_Error: " + std::string(SDL_GetError()));
	}

	vkSurface = surface;
	return surface;
}

void SdlWindow::toggleBorderlessFullscreen() {
	Uint32 flags = SDL_GetWindowFlags(handle);
	bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	if (!is_fullscreen) {
		if (!SDL_SetWindowFullscreen(handle, true)) {
			logError("Failed to enter fullscreen: {}", "SdlWindow", SDL_GetError());
		} else {
			logInfo("Entered borderless fullscreen");
		}
	} else {
		if (!SDL_SetWindowFullscreen(handle, false)) {
			logError("Failed to exit fullscreen: {}", "SdlWindow", SDL_GetError());
		} else {
			logInfo("Exited borderless fullscreen");
		}
	}
}
