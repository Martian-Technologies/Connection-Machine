#ifndef app_h
#define app_h

#include <SDL3/SDL_init.h>

class SdlWindow;
class SDL_Window;

namespace App {
	void init();
	void kill();
	template<class WindowType, class... Args>
	requires std::derived_from<WindowType, SdlWindow>
	std::shared_ptr<SdlWindow> makeWindow(Args&&... args);
	void registerWindow(std::shared_ptr<SdlWindow>& window);

	double getDetlaTime();
	void handleEvent(SDL_Event& event);
	SDL_AppResult iterate();
	void startTryingToQuit();
	void stopTryingToQuit();

	void doRunOnMainForThread(std::thread::id threadId);
	// can fail is app is getting killed
	bool runOnMain_blocking(std::function<void()> func);
	void runOnMain(std::function<void()> func);

	nlohmann::json dumpState();
};

template<class WindowType, class... Args>
requires std::derived_from<WindowType, SdlWindow>
std::shared_ptr<SdlWindow> App::makeWindow(Args&&... args) {
	std::shared_ptr<SdlWindow> window = std::make_shared<WindowType>(std::forward<Args>(args)...);
	registerWindow(window);
	return window;
}


#endif /* app_h */
