#include "sdlWindow.h"
#include "util/fastMath.h"
#include "app.h"

#include <SDL3/SDL_events.h>

bool resizingEventWatcher(void* data, SDL_Event* event) {
	if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
		if (((SdlWindow*)data)->getHandle() == win) {
			if (((SdlWindow*)data)->doRecieveEvent) {
				((SdlWindow*)data)->doRecieveEvent(*event);
			}
		}
	}
	return 0;
}

SdlWindow::SdlWindow(const std::string& name, unsigned int width, unsigned int height) {
	logInfo("Creating SDL window...");

	handle = SDL_CreateWindow(name.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!handle)
	{
		throwFatalError("SDL could not create window! SDL_Error: " + std::string(SDL_GetError()));
	}

	SDL_AddEventWatch(resizingEventWatcher, this);

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

bool SdlWindow::recieveEvent(SDL_Event& event) {
	if (doRecieveEvent) {
		return doRecieveEvent(event);
	}
	if (isThisMyEvent(event)) {
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			App::get().deregisterWindow(this);
		}
		return true;
	}

	return false;
}

void SdlWindow::sendKillEvent() {
	SDL_Event e;
	SDL_zero(e);

	e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
	e.window.windowID = SDL_GetWindowID(handle);

	SDL_PushEvent(&e);
}

void SdlWindow::instantKillEvent() {
	SDL_Event e;
	SDL_zero(e);

	e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
	e.window.windowID = SDL_GetWindowID(handle);

	recieveEvent(e);
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
