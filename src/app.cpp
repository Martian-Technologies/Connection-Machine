#include "app.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "RmlUi/Debugger/Debugger.h"
#include "gui/helper/saveCallback.h"
#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "network/network.h"
#include "util/version.h"
// #include "computerAPI/directoryManager.h" // TEMPORARY

#include <SDL3/SDL.h>

std::optional<App> appSingleton;

App& App::get() {
	if (!appSingleton) {
		logInfo("Creating App", "App");
		appSingleton.emplace();
		appSingleton->newMainWindow();
	}
	return *appSingleton;
}

void App::kill() {
	logInfo("Killing App", "App");
	appSingleton->preShutdownStep();
	appSingleton.reset();
}

App::App() {
	logInfo("Initializing App", "App");
	rml.emplace(&rmlSystemInterface, &rmlRenderInterface);
	logInfo("App initialized", "App");
}

void App::preShutdownStep() {
	logInfo("Shutting down App", "App");
	for (std::shared_ptr<SdlWindow>& sdlWindow : sdlWindows) {
		if (sdlWindow) sdlWindow->instantKillEvent(); // make sure window knows its going
	}
	sdlWindows.clear();
	if (windows.size() != windowsToDestroy.size()) {
		logError("Not all main windows were cleaned up. Clearing up {} extra.", "App", windows.size() - windowsToDestroy.size());
	}
	windows.clear();
	rml.reset();
	MainRenderer::kill();
	Network::kill();
}

App::~App() { logInfo("App shut down", "App"); }

std::shared_ptr<SdlWindow> App::registerWindow(const std::string& windowName) { return sdlWindows.emplace_back(std::make_shared<SdlWindow>(windowName)); }
std::shared_ptr<SdlWindow> App::registerWindow(const std::string& windowName, unsigned int width, unsigned int height) {
	return sdlWindows.emplace_back(std::make_shared<SdlWindow>(windowName, width, height));
}

void App::deregisterWindow(std::shared_ptr<SdlWindow>& sdlWindow) {
	auto iter = std::find(sdlWindows.begin(), sdlWindows.end(), sdlWindow);
	if (iter != sdlWindows.end()) sdlWindows.erase(iter);
}

void App::deregisterWindow(const SdlWindow* sdlWindow) {
	auto iter = std::find_if(sdlWindows.begin(), sdlWindows.end(), [sdlWindow](const std::shared_ptr<SdlWindow>& a) { return a.get() == sdlWindow; });
	if (iter != sdlWindows.end()) sdlWindows.erase(iter);
}

void App::newMainWindow() {
	logInfo("Creating new MainWindow", "App");
	windows.push_back(std::make_unique<MainWindow>(environment));
	newlyCreatedWindowsNext.push_back(windows.back().get());
}

bool App::closeMainWindow(const MainWindow* mainWindow) {
	logInfo("Closing MainWindow", "App");
	if (windows.size() == windowsToDestroy.size() + 1) {
		startTryingToQuit();
		if (tasksToFinishToQuit == 0) {
			windowsToDestroy.push_back(mainWindow);
			return true;
		}
		return false;
	} else {
		windowsToDestroy.push_back(mainWindow);
		return true;
	}
}

#ifdef TRACY_PROFILER
const char* const addLoopTracyName = "appLoop";
#endif

void App::runLoop() {
	logInfo("Starting App loop", "App");
	Network::get().checkForUpdates(get().windows[0]->getPopUpManager());
	running = true;
	while (running) {
		// do texture updates
		environment.getBlockRenderDataFeeder().doBlockTextureUpdates();

		// Wait for the next event (so we don't broork the cpu)
		bool gotEvent = SDL_WaitEventTimeout(nullptr, 50);
#ifdef TRACY_PROFILER
		FrameMarkStart(addLoopTracyName);
#endif
		if (gotEvent) {
			// process events (TODO - should probably just have a map of window ids to windows)
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_EVENT_QUIT: {
					// Main application quit (eg. ctrl-c in terminal)
					startTryingToQuit();
					break;
				}
				// case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
				// 	// Single window was closed, check which window was closed and remove it
				// 	for (auto itr = sdlWindows.begin(); itr != sdlWindows.end(); ++itr) {
				// 		std::shared_ptr<SdlWindow> sdlWindow = *itr;
				// 		if (sdlWindow->recieveEvent(event)) {
				// 			deregisterWindow(sdlWindow);
				// 			break;
				// 		}
				// 	}
				// 	break;
				// }
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
					std::vector<SdlWindow*> windowsToSendEvents;
					for (unsigned int i = 0; i < sdlWindows.size(); ++i) {
						windowsToSendEvents.push_back(sdlWindows[i].get());
					}
					for (unsigned int i = 0; i < windowsToSendEvents.size(); ++i) {
						if (std::any_of(sdlWindows.begin(), sdlWindows.end(), [windowPtr = windowsToSendEvents[i]](const std::shared_ptr<SdlWindow>& a) {
							return a.get() == windowPtr;
						})) {
							windowsToSendEvents[i]->recieveEvent(event);
						}
					}
				}
				}
			}
			for (unsigned int i = 0; i < windowsToDestroy.size(); ++i) {
				auto iter = std::find_if(windows.begin(), windows.end(), [mainWindow = windowsToDestroy[i]](const std::unique_ptr<MainWindow>& a) {
					return a.get() == mainWindow;
				});
				if (iter == windows.end()) {
					logError("Could not find MainWindow when trying to close it.", "App");
					return;
				}
				windows.erase(iter);
			}
			windowsToDestroy.clear();
		}
		// tell all windows to update rml
		for (auto& sdlWindow : sdlWindows) {
			sdlWindow->render();
		}
		for (auto* window : newlyCreatedWindows) {
			for (auto circuitViewWidget : window->getCircuitViewWidgets()) {
				circuitViewWidget->handleResize();
			}
		}
		newlyCreatedWindows = std::move(newlyCreatedWindowsNext);
		newlyCreatedWindowsNext.clear();

		for (const std::function<void()>& function : functionsToRunAtEndOfUpdate) {
			function();
		}
		functionsToRunAtEndOfUpdate.clear();

#ifdef TRACY_PROFILER
		FrameMarkEnd(addLoopTracyName);
#endif
	}
}

void App::startTryingToQuit() {
	if (tryingToQuit) return;
	// { // TEMPORARY
	// 	// do dumpstate and into file
	// 	std::string dumpStateStr = dumpState().dump(1, '\t');
	// 	std::string path = (DirectoryManager::getConfigDirectory() / ("dumpstate.json")).string();
	// 	std::ofstream file(path);
	// 	if (file.is_open()) {
	// 		file << dumpStateStr;
	// 		file.close();
	// 		logInfo("Dumped state to {}", "App", path);
	// 	} else {
	// 		logError("Could not open {} to dump state!", "App", path);
	// 	}
	// }
	tasksToFinishToQuit = 0;
	tryingToQuit = true;
	auto windowIter = windows.begin();
	while (&windowIter->get()->getEnvironment() != &environment) {
		++windowIter;
		if (windowIter == windows.end()) {
			logError("Could not find window to save with! TODO: create new window that saves can happen in!");
			return;
		}
	}
	for (auto& fileData : environment.getCircuitFileManager().getAllFiles()) {
		if (fileData.second.lastSavedEdit.empty()) continue;
		std::string message = "Do you want to save:\n";
		std::vector<const std::string*> toSave;
		for (auto lastSavedEdit : fileData.second.lastSavedEdit) {
			const SharedCircuit& circuit = environment.getBackend().getCircuitManager().getCircuit(lastSavedEdit.first);
			if (!circuit->isEditable()) continue;
			if (lastSavedEdit.second == circuit->getEditCount()) continue;
			message += circuit->getCircuitName() + "\n";
			toSave.emplace_back(&circuit->getUUID());
		}
		if (toSave.size() == 0) continue;
		++tasksToFinishToQuit;
		windowIter->get()->getPopUpManager().addOptionsPopUp(
			message,
			{ std::make_pair(
				  "Save",
				  [filePath = fileData.second.fileLocation, this]() {
			logInfo("Saving {}", "", filePath);
			environment.getCircuitFileManager().saveFile(filePath);
			--tasksToFinishToQuit;
			if (tasksToFinishToQuit == 0) running = false;
		}
			  ),
			  std::make_pair(
				  "Dont Save",
				  [this]() {
			logInfo("\"Dont Save\" option picked.");
			--tasksToFinishToQuit;
			if (tasksToFinishToQuit == 0) running = false;
		}
			  ),
			  std::make_pair("Cancel", [this]() {
			logInfo("Canceling close.");
			stopTryingToQuit();
		}) }
		);
	}
	for (auto& circuit : environment.getBackend().getCircuitManager().getCircuits()) {
		if (environment.getCircuitFileManager().getSavePath(circuit.second->getUUID())) continue;
		if (!circuit.second->isEditable()) continue;
		++tasksToFinishToQuit;
		windowIter->get()->getPopUpManager().addOptionsPopUp(
			"Do you want to save: " + circuit.second->getCircuitName(),
			{ std::make_pair(
				  "Save",
				  [window = windowIter->get(), uuid = circuit.second->getUUID(), this]() {
			logInfo("Saving circuit {}", "", uuid);
			static const SDL_DialogFileFilter filters[] = { { "Circuit Files", "cir" } };
			std::pair<CircuitFileManager*, std::string>* data = new std::pair<CircuitFileManager*, std::string>(&environment.getCircuitFileManager(), uuid);
			SDL_ShowSaveFileDialog([](void* userData, const char* const* filePaths, int filter) {
				SaveCallback(userData, filePaths, filter);
				--(App::get().tasksToFinishToQuit);
				if (App::get().tasksToFinishToQuit == 0) App::get().running = false;
			}, data, nullptr, filters, 1, nullptr);
		}
			  ),
			  std::make_pair(
				  "Dont Save",
				  [this]() {
			logInfo("\"Dont Save\" option picked.");
			--tasksToFinishToQuit;
			if (tasksToFinishToQuit == 0) running = false;
		}
			  ),
			  std::make_pair("Cancel", [this]() {
			logInfo("Canceling close.");
			stopTryingToQuit();
		}) }
		);
	}
	if (tasksToFinishToQuit == 0) running = false;
}

void App::stopTryingToQuit() { tryingToQuit = false; }

nlohmann::json App::dumpState() const {
	nlohmann::json stateJson;
	stateJson["environment"] = environment.dumpState();
	stateJson["version"] = getCurrentVersion().toString();
	return stateJson;
}

void App::launchRmlDebugger(Rml::Context* rmlContext) {
	if (debuggingWindow) {
		Rml::Debugger::SetContext(rmlContext);
		return;
	}
	SdlWindow* sdlWindow = App::get().registerWindow("Debugger", 350, 800).get();
	debuggingWindow = sdlWindow;
	WindowId windowId = MainRenderer::get().registerWindow(sdlWindow);
	Rml::Context* debuggerRmlContext = Rml::CreateContext("Debugger", Rml::Vector2i(sdlWindow->getSize().first, sdlWindow->getSize().second));
	if (debuggerRmlContext) {
		// Rml::ElementDocument* rmlDocument = debuggerRmlContext->CreateDocument();
		// rmlDocument->AppendChild(rmlDocument->CreateElement("div"));
		Rml::Debugger::Initialise(debuggerRmlContext);
		Rml::Debugger::SetContext(rmlContext);
		Rml::Debugger::SetVisible(true);
		sdlWindow->setRenderFunction([windowId, debuggerRmlContext]() {
			RmlRenderInterface* rmlRenderInterface = dynamic_cast<RmlRenderInterface*>(Rml::GetRenderInterface());
			if (rmlRenderInterface) {
				debuggerRmlContext->Update();
				rmlRenderInterface->setWindowToRenderOn(windowId);
				MainRenderer::get().prepareForRmlRender(windowId);
				debuggerRmlContext->Render();
				MainRenderer::get().endRmlRender(windowId);
			}
		});
		sdlWindow->setRecieveEventFunction([this, windowId, debuggerRmlContext, sdlWindow](SDL_Event& event) {
			if (sdlWindow->isThisMyEvent(event)) {
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
					Rml::RemoveContext(debuggerRmlContext->GetName());
					MainRenderer::get().deregisterWindow(windowId);
					App::get().deregisterWindow(sdlWindow);
					this->debuggingWindow = nullptr;
					Rml::Debugger::Shutdown();
					return true;
				}

				RmlSDL::InputEventHandler(debuggerRmlContext, sdlWindow->getHandle(), event, sdlWindow->getWindowScalingSize());

				// let renderer know we if resized the window
				if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
					MainRenderer::get().resizeWindow(windowId, { event.window.data1, event.window.data2 });
					debuggerRmlContext->Update();
				}
				return true;
			}
			return false;
		});
	}
}

void App::killRmlDebugger() {
	if (debuggingWindow) {
		debuggingWindow->sendKillEvent();
	}
}
