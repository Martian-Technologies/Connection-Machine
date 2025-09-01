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
	App();

	void runLoop();

private:
	Environment environment;

	RmlRenderInterface rmlRenderInterface;
	RmlSystemInterface rmlSystemInterface;

	SdlInstance sdl;
	RmlInstance rml;

	std::vector<std::unique_ptr<MainWindow>> windows; // we could make this just a vector later, I don't want to deal with moving + threads
	bool running = false;
};

#endif /* app_h */
