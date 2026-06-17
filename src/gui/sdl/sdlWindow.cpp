#include "sdlWindow.h"
#include "util/fastMath.h"
#include "gpu/mainRenderer.h"
#include "app.h"

#include <SDL3/SDL_events.h>

bool resizingEventWatcher(void* data, SDL_Event* event) {
	if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
		if (((SdlWindow*)data)->getHandlePtr() == win) {
			if (((SdlWindow*)data)->recieveEvent(*event)) {
				((SdlWindow*)data)->recieveEvent(*event);
			}
		}
	}
	return 0;
}

SdlWindow::SdlWindow(const std::string& name, unsigned int width, unsigned int height) {
	logInfo("Creating SDL window...", "SdlWindow");

	handle = SDL_CreateWindow(name.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!handle) {
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

	renderingMux.lock();
	windowId = MainRenderer::get().registerWindow(this);
}

// SdlWindow::SdlWindow(SDL_Window* handle) : handle(handle) {
// 	logInfo("Using pre created SDL window...", "SdlWindow");

// 	if (!handle) {
// 		throwFatalError("SdlWindow can't use NULL window!");
// 	}

// 	SDL_AddEventWatch(resizingEventWatcher, this);

// 	int winW, winH, drawW, drawH;
// 	SDL_GetWindowSize(handle, &winW, &winH);
// 	SDL_GetWindowSizeInPixels(handle, &drawW, &drawH);

// 	float scaleX = (float)drawW / winW;
// 	float scaleY = (float)drawH / winH;

// 	if (!approx_equals(scaleX, scaleY)) {
// 		logError("width scale not the same x={} y={}", "SdlWindow", scaleX, scaleY);
// 	}

// 	windowScalingSize = scaleX;
// }

SdlWindow::~SdlWindow() {
	if (killed) return;
	kill(true);
}

std::string SdlWindow::getName() const {
	if (killed) {
		logError("SdlWindow is killed and does not have name!", "SdlWindow::getName");
		return "Killed window!";
	}
	return SDL_GetWindowTitle(handle);
}

std::pair<uint32_t, uint32_t> SdlWindow::getSize() const {
	if (killed) {
		logError("SdlWindow is killed and does not have size!", "SdlWindow::getSize");
		return {1, 1};
	}
	if (!handle) return {1, 1};
	int w, h;
	SDL_GetWindowSize(handle, &w, &h);
	return { w, h };
}

bool SdlWindow::recieveEvent(SDL_Event& event) {
	if (killed) {
		logError("SdlWindow is killed and cant recieve events!", "SdlWindow::recieveEvent");
		return false;
	}
	if (isThisMyEvent(event)) {
		if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
			std::lock_guard mux(renderingMux);
			MainRenderer::get().resizeWindow(windowId, { event.window.data1, event.window.data2 });
		} else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			kill(false); // kill with lock renderingMux
		} else {
			std::lock_guard mux(renderingMux);
			processEvent(event);
		}
		return true;
	}

	return false;
}

bool SdlWindow::isThisMyEvent(const SDL_Event& event) const {
	if (killed) {
		logError("SdlWindow is killed and cant do event stuff!", "SdlWindow::isThisMyEvent");
		return false;
	}
	if (!handle) return false;
	if (event.type == 2050) return true; // the fuck is this? - jack quay jamison
	return SDL_GetWindowFromEvent(&event) == handle;
}

VkSurfaceKHR SdlWindow::createVkSurface(VkInstance instance) {
	if (killed) {
		logError("SdlWindow is killed and does not have a vk surface!", "SdlWindow::createVkSurface");
		return nullptr;
	}
	if (!handle) return nullptr;

	if (vkSurface.has_value()) return vkSurface.value();

	vkInstance = instance;

	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(handle, instance, nullptr, &surface)) {
		logError("SDL could not create vulkan surface! SDL_Error: {}", "SdlWindow", std::string(SDL_GetError()));
		return nullptr;
	}

	vkSurface = surface;
	return surface;
}

void SdlWindow::doRendering() {
	std::lock_guard mux(renderingMux);
	if (killed) {
		// logError("Can't render killed SdlWindow!", "SdlWindow::doRendering");
		return;
	}
	render();
}

void SdlWindow::toggleBorderlessFullscreen() {
	if (killed) {
		logError("SdlWindow is killed and cant be set to full screen!", "SdlWindow::toggleBorderlessFullscreen");
		return;
	}
	Uint32 flags = SDL_GetWindowFlags(handle);
	bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	std::lock_guard mux(renderingMux);
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

nlohmann::json SdlWindow::dumpWindowState() const {
	if (killed) return "Killed window!";
	nlohmann::json json;
	json["name"] = getName();
	json["size"] = getSize();
	json["windowScalingSize"] = windowScalingSize;
	json["hasVkSurface"] = vkSurface.has_value();
	json["windowSubclass"] = dumpState();
	return json;
}

bool SdlWindow::kill(bool forced) {
	if (killed) {
		logError("Cant kill already killed window!", "SdlWindow::kill");
		return true;
	}
	{
		std::lock_guard mux(renderingMux);
		bool subclassKilled = killWindow(forced);
		if (forced) assert(subclassKilled);
		if (!(forced || subclassKilled)) return false;
		killed = true; // set killed afterward to allow the subclasses to access values one last time
	}
	logInfo("Destroying SDL window...");
	MainRenderer::get().deregisterWindow(windowId);
	SDL_RemoveEventWatch(resizingEventWatcher, this);
	if (vkSurface.has_value()) {
		SDL_Vulkan_DestroySurface(vkInstance, vkSurface.value(), nullptr);
		vkSurface.reset();
	}
	SDL_DestroyWindow(handle);
	handle = nullptr;
	return true;
}