#include "app.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include <SDL3/SDL.h>

#include "gui/sdl/sdlWindow.h"
#include "network/network.h"
#include "util/version.h"
#include "gpu/renderer/imgui/imGuiRenderer.h"
#include "gui/sdl/sdlInstance.h"
#include "gpu/mainRenderer.h"
#include "gui/mainWindow/mainWindow.h"

std::thread::id mainThreadId = std::this_thread::get_id();

// gloable app values
std::optional<SdlInstance> sdl;
std::set<std::shared_ptr<SdlWindow>> windows;
bool running = false;
bool tryingToQuit = false;
std::mutex runOnMainFunctionsMux;
std::vector<std::pair<std::thread::id, std::function<void()>>> runOnMainFunctions;
std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdateTime;
std::chrono::nanoseconds detlaTime;
std::atomic<bool> killingApp = false;

void App::init() {
	assert(mainThreadId == std::this_thread::get_id());
	running = true;
	if (windows.empty()) {
		logError("Cant send update popup when no windows exist!");
	} else {
		for (auto& window : windows) {
			MainWindow* mainWindow = dynamic_cast<MainWindow*>(window.get());
			if (mainWindow != nullptr) {
				Network::get().checkForUpdates(*mainWindow);
				break;
			}
		}
	}
	lastUpdateTime = std::chrono::high_resolution_clock::now();
	SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "120");
}

void App::kill() {
	assert(mainThreadId == std::this_thread::get_id());
	logInfo("Killing App", "App");
	for (std::shared_ptr<SdlWindow> window : windows) {
		if (!window->isKilled()) {
			logError("SDL window \"{}\" was not killed.", "App", window->getName());
		}
	}
	windows.clear();
	killingApp.store(true);
	MainRenderer::kill();
	Network::kill();
	sdl.reset();
	std::lock_guard lock(runOnMainFunctionsMux);
	running = false;
	assert(runOnMainFunctions.empty());
}

void App::registerWindow(std::shared_ptr<SdlWindow>& window) {
	assert(mainThreadId == std::this_thread::get_id());
	windows.emplace(window);
	window->renderingMux.unlock();
}

double App::getDetlaTime() {
	assert(mainThreadId == std::this_thread::get_id());
	return ((double)detlaTime.count()) / 1000000000.;
}

void App::handleEvent(SDL_Event& event) {
	assert(mainThreadId == std::this_thread::get_id());
	ImGuiRenderer::allProcessEvent(event);
	switch (event.type) {
	case SDL_EVENT_QUIT: {
		// Main application quit (eg. ctrl-c in terminal)
		startTryingToQuit();
		break;
	}
	default: {
		// Send event to all windows
		for (const std::shared_ptr<SdlWindow>& window : windows) {
			if (window->isKilled()) break;
			window->recieveEvent(event);
		}
	}
	}
}

unsigned int i = 0;

SDL_AppResult App::iterate() {
	assert(mainThreadId == std::this_thread::get_id());
	for (const std::shared_ptr<SdlWindow>& window : windows) {
		if (window->isKilled()) continue;
		window->doUpdate();
	}
	{
		runOnMainFunctionsMux.lock();
		while (!runOnMainFunctions.empty()) {
			auto func = runOnMainFunctions.back();
			runOnMainFunctions.pop_back();
			runOnMainFunctionsMux.unlock();
			func.second();
			runOnMainFunctionsMux.lock();
		}
		runOnMainFunctionsMux.unlock();
		assert(runOnMainFunctions.empty());
	}
	std::erase_if(windows, [](const std::shared_ptr<SdlWindow>& window){ return window->isKilled(); });
	if (windows.empty()) return SDL_AppResult::SDL_APP_FAILURE;
	return SDL_AppResult::SDL_APP_CONTINUE;
}

void App::startTryingToQuit() {
	assert(mainThreadId == std::this_thread::get_id());
	for (std::shared_ptr<SdlWindow> window : windows) {
		if (window->isKilled()) continue;
		if (!window->kill(false)) {
			return; // window canceled quit
		}
	}
}

void App::stopTryingToQuit() {
	assert(mainThreadId == std::this_thread::get_id());
	tryingToQuit = false;
}

void App::doRunOnMainForThread(std::thread::id threadId) {
	std::lock_guard lock(runOnMainFunctionsMux);
	for (int i = 0; i < runOnMainFunctions.size(); i++) {
		if (runOnMainFunctions[i].first == threadId) {
			runOnMainFunctions[i].second();
			if (i + 1 == runOnMainFunctions.size()) {
				runOnMainFunctions.pop_back();
				return;
			} else {
				runOnMainFunctions[i] = runOnMainFunctions.back();
				runOnMainFunctions.pop_back();
				i--;
			}
		}
	}
}

bool App::runOnMain_blocking(std::function<void()> func) {
#ifdef TRACY_PROFILER
	ZoneScoped; // allow us to see it stuff is slow because of this.
#endif
	// if this happens to be the main thread then we can just run the function
	if (mainThreadId == std::this_thread::get_id()) {
		func();
		return true;
	}
	if (killingApp.load()) {
		std::stringstream ss;
		ss << std::this_thread::get_id();
		logWarning("runOnMain_blocking called from thread {} while app was closing! This may be a error.", "App::runOnMain_blocking", ss.str());

		return false;
	}
	std::atomic<bool> isDone{false};
	{
		std::lock_guard lock(runOnMainFunctionsMux);
		runOnMainFunctions.emplace_back(std::this_thread::get_id(), [isDone = &isDone, func](){
			func();
			isDone->store(true);
			isDone->notify_all();
		});
	}
	isDone.wait(false);
	return false;
}

void App::runOnMain(std::function<void()> func) {
	// if this happens to be the main thread then we can just run the function
	if (mainThreadId == std::this_thread::get_id()) {
		func();
		return;
	}
	runOnMainFunctions.emplace_back(std::this_thread::get_id(), func);
}

nlohmann::json App::dumpState() /* GCOVR_EXCL_FUNCTION */ {
	assert(mainThreadId == std::this_thread::get_id());
	nlohmann::json stateJson;
	stateJson["windows"] = nlohmann::json::array();
	for (const std::shared_ptr<SdlWindow>& window : windows) {
		stateJson["windows"] = window->dumpWindowState();
	}
	stateJson["version"] = getCurrentVersion().toString();
	return stateJson;
}
