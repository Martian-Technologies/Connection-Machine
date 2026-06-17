#include "sdlInstance.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>

#define SDL_GESTURE_IMPLEMENTATION 1
// #include "SDL_gesture.h"

SdlInstance::SdlInstance() {
	logInfo("Initializing SDL...");

#if defined(unix) && !(defined(__APPLE__) || defined(__MACH__))
	// If we have wayland, enable wayland for SDL (thanks Riley J. Beckett (nathanial b))
	int numberOfVideoDrivers = SDL_GetNumVideoDrivers();
	for (int i = 0; i < numberOfVideoDrivers; i++) {
		if (std::string(SDL_GetVideoDriver(i)) == "wayland") {
			SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "wayland", SDL_HINT_OVERRIDE);
			break;
		}
	}
#endif

	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		throwFatalError("SDL could not initialize! SDL_Error: " + std::string(SDL_GetError()));
	}

	// if (Gesture_Init() == -1) {
	// 	throw std::runtime_error("SDL Gestures could not initialize! SDL_Error: " + std::string(SDL_GetError()));
	// }

	// Submit click events when focusing the window.
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");


}

SdlInstance::~SdlInstance() { // dont do anything here because any other part of the code could have been destroyed
	// Gesture_Quit();
	SDL_Quit();
}

std::vector<SDL_Event> SdlInstance::pollEvents() {
	std::vector<SDL_Event> events;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		events.push_back(event);
	}

	return events;
}
