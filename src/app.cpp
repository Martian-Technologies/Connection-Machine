#include "app.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "gui/mainWindow/circuitView/circuitViewWidget.h"

std::optional<App> appSingleton;

App& App::get() {
	if (!appSingleton) {
		appSingleton.emplace();
		appSingleton->windows.push_back(std::make_unique<MainWindow>(&appSingleton->environment));
	}
	return *appSingleton;
}

void App::kill() {
	appSingleton.reset();
}

App::App() : rml(&rmlSystemInterface, &rmlRenderInterface) {}

std::shared_ptr<SdlWindow> App::registerWindow(const std::string& windowName) {
	return sdlWindows.emplace_back(std::make_shared<SdlWindow>(std::move(windowName)));
}

void App::deregisterWindow(std::shared_ptr<SdlWindow>& sdlWindow) {
	auto iter = std::find(sdlWindows.begin(), sdlWindows.end(), sdlWindow);
	if (iter != sdlWindows.end()) sdlWindows.erase(iter);
}
void App::deregisterWindow(const SdlWindow* sdlWindow) {
	auto iter = std::find_if(sdlWindows.begin(), sdlWindows.end(), [sdlWindow](const std::shared_ptr<SdlWindow>& a){ return a.get() == sdlWindow; });
	if (iter != sdlWindows.end()) sdlWindows.erase(iter);
}

#ifdef TRACY_PROFILER
const char* const addLoopTracyName = "appLoop";
#endif

void App::runLoop() {
	bool firstPass = true;
	running = true;
	while (running) {
		// Wait for the next event (so we don't broork the cpu)
		SDL_WaitEvent(nullptr);

#ifdef TRACY_PROFILER
		FrameMarkStart(addLoopTracyName);
#endif
		// process events (TODO - should probably just have a map of window ids to windows)
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT: {
				// Main application quit (eg. ctrl-c in terminal)
				running = false;
				break;
			}
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
				// Single window was closed, check which window was closed and remove it
				for (auto itr = sdlWindows.begin(); itr != sdlWindows.end(); ++itr) {
					std::shared_ptr<SdlWindow> sdlWindow = *itr;
					if (sdlWindow->recieveEvent(event)) {
						deregisterWindow(sdlWindow);
						break;
					}
				}
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED: {
				// Window focus switched, check which window gained focus
				for (auto& sdlWindow : sdlWindows) {
					if (sdlWindow->recieveEvent(event)) {
						// tell system interface about focus change
						rmlSystemInterface.SetWindow(sdlWindow->getHandle());
						break;
					}
				}
				break;
			}
			default: {
				// Send event to all windows
				for (auto& sdlWindow : sdlWindows) {
					sdlWindow->recieveEvent(event);
				}
			}
			}
		}

		// tell all windows to update rml
		for (auto& sdlWindow : sdlWindows) {
			sdlWindow->render();
		}
		if (firstPass) {
			firstPass = false;
			for (auto& window : windows) {
				for (auto circuitViewWidget : window->getCircuitViewWidgets()) {
					circuitViewWidget->handleResize();
				}
			}
		}
#ifdef TRACY_PROFILER
		FrameMarkEnd(addLoopTracyName);
#endif
	}
}
