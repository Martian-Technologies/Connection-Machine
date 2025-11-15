#ifndef app_h
#define app_h

#include "environment/environment.h"
#include "gui/mainWindow/mainWindow.h"
#include "gui/rml/rmlInstance.h"
#include "gui/rml/rmlRenderInterface.h"
#include "gui/rml/rmlSystemInterface.h"
#include "gui/sdl/sdlInstance.h"

class App {
public:
	static App& get();
	static void kill();

	// do not call
	App();
	~App();
	void preShutdownStep();

	std::shared_ptr<SdlWindow> registerWindow(const std::string& windowName);
	std::shared_ptr<SdlWindow> registerWindow(const std::string& windowName, unsigned int width, unsigned int height);
	void deregisterWindow(std::shared_ptr<SdlWindow>& sdlWindow);
	void deregisterWindow(const SdlWindow* sdlWindow);
	void queForEndOfUpdate(std::function<void()> function) { functionsToRunAtEndOfUpdate.push_back(function); }

	void newMainWindow();
	bool closeMainWindow(const MainWindow* mainWindow);

	void runLoop();
	void startTryingToQuit();
	void stopTryingToQuit();

	nlohmann::json dumpState() const;

	void launchRmlDebugger(Rml::Context* rmlContext);
	void killRmlDebugger();

private:
	SdlWindow* debuggingWindow = nullptr;
	Environment environment {true};

	RmlRenderInterface rmlRenderInterface;
	RmlSystemInterface rmlSystemInterface;

	SdlInstance sdl;
	std::optional<RmlInstance> rml;

	std::vector<std::shared_ptr<SdlWindow>> sdlWindows;
	std::vector<std::unique_ptr<MainWindow>> windows; // we could make this just a vector later, I don't want to deal with moving + threads
	std::vector<const MainWindow*> windowsToDestroy;
	std::vector<MainWindow*> newlyCreatedWindowsNext;
	std::vector<MainWindow*> newlyCreatedWindows;
	std::vector<std::function<void()>> functionsToRunAtEndOfUpdate;
	bool running = false;
	bool tryingToQuit = false;
	unsigned int tasksToFinishToQuit = 0;
};

#endif /* app_h */
