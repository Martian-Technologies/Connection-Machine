#ifndef window_h
#define window_h

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>

#include "gui/sdl/sdlWindow.h"

#include "circuitView/simControlsManager.h"
#include "cornerLog.h"
#include "gui/helper/keybindHandler.h"
#include "gui/mainWindow/menuBar/menuBar.h"
#include "popUps/popUpManager.h"
#include "settingsWindow/settingsWindow.h"
#include "sideBar/icEditor/blockCreationWindow.h"
#include "sideBar/selector/selectorWindow.h"
#include "sideBar/simulation/evalWindow.h"
#include "tools/toolManagerManager.h"

class CircuitViewWidget;
class Environment;

class MainWindow {
public:
	MainWindow(Environment& environment);
	~MainWindow();

	// no copy
	MainWindow(const MainWindow&) = delete;
	MainWindow& operator=(const MainWindow&) = delete;

	Environment& getEnvironment() { return environment; }
	const Environment& getEnvironment() const { return environment; }

	SimControlsManager* getSimControlsManager() { return simControlsManager.has_value() ? &simControlsManager.value() : nullptr; }

	ToolManagerManager& getToolManagerManager() { return toolManagerManager; }
	const ToolManagerManager& getToolManagerManager() const { return toolManagerManager; }

	Rml::ElementDocument* getRmlDocument() { return rmlDocument; }
	const Rml::ElementDocument* getRmlDocument() const { return rmlDocument; }

	PopUpManager& getPopUpManager() { return popUpManager; }

	// logging
	// CornerLog& getCornerLog() { return cornerLog.value(); } // dont need this with the other functions here
	void log(const std::string& message) { cornerLog->log(message); }
	template <typename... Args>
	void log(const fmt::format_string<Args...>& formatString, Args&&... args) {
		cornerLog->log(formatString, std::forward<Args>(args)...);
	}
	void logError(const std::string& message) { cornerLog->logError(message); }
	template <typename... Args>
	void logError(const fmt::format_string<Args...>& formatString, Args&&... args) {
		cornerLog->logError(formatString, std::forward<Args>(args)...);
	}

	bool recieveEvent(SDL_Event& event);
	void updateRml();

	inline SDL_Window* getSdlWindow() { return sdlWindow->getHandle(); };
	inline float getSdlWindowScalingSize() const { return sdlWindow->getWindowScalingSize(); }
	inline std::vector<std::shared_ptr<CircuitViewWidget>> getCircuitViewWidgets() { return circuitViewWidgets; };
	inline std::shared_ptr<CircuitViewWidget> getCircuitViewWidget(unsigned int i) { return circuitViewWidgets[i]; };
	inline std::shared_ptr<CircuitViewWidget> getActiveCircuitViewWidget() { return activeCircuitViewWidget; };

	// void addCircuitViewWidget() // once we can change element that it is attached to
	void createCircuitViewWidget(Rml::Element* element);

	void setGlobalCssProperty(const std::string& property, const std::string& value);

private:
	void offsetUiScale(double delta);
	void applyUiScale(float scale);

	WindowId windowId;
	Environment& environment;

	// inputs and tools
	KeybindHandler keybindHandler;
	ToolManagerManager toolManagerManager;
	float uiScale = 1.0f;
	bool uiScaleSettingUpdateInProgress = false;
	static constexpr double kUiScaleStep = 0.1;
	static constexpr double kUiScaleMin = 0.5;
	static constexpr double kUiScaleMax = 3.0;

	// widgets
	std::optional<SelectorWindow> selectorWindow;
	std::optional<EvalWindow> evalWindow;
	std::optional<BlockCreationWindow> blockCreationWindow;
	std::optional<SimControlsManager> simControlsManager;
	std::optional<SettingsWindow> settingsWindow;
	std::optional<MenuBar> menuBar;
	std::optional<CornerLog> cornerLog;

	std::vector<std::pair<std::string, const std::vector<std::pair<std::string, std::function<void()>>>>> popUpsToAdd;

	// circuit view widget
	std::shared_ptr<CircuitViewWidget> activeCircuitViewWidget;
	std::vector<std::shared_ptr<CircuitViewWidget>> circuitViewWidgets;

	// rmlui and sdl
	PopUpManager popUpManager;
	Rml::Context* rmlContext;
	Rml::ElementDocument* rmlDocument;
	std::shared_ptr<SdlWindow> sdlWindow;
};

#endif /* window_h */
