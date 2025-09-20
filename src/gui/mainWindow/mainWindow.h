#ifndef window_h
#define window_h

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>

#include "gui/sdl/sdlWindow.h"

#include "tools/toolManagerManager.h"
#include "sideBar/icEditor/blockCreationWindow.h"
#include "circuitView/simControlsManager.h"
#include "sideBar/selector/selectorWindow.h"
#include "sideBar/simulation/evalWindow.h"
#include "gui/helper/keybindHandler.h"

class CircuitViewWidget;
class Environment;

class MainWindow {
public:
	MainWindow(Environment* environment);
	~MainWindow();

	// no copy
	MainWindow(const MainWindow&) = delete;
	MainWindow& operator=(const MainWindow&) = delete;

public:
	bool recieveEvent(SDL_Event& event);
	void updateRml();

	inline SDL_Window* getSdlWindow() { return sdlWindow->getHandle(); };
	inline float getSdlWindowScalingSize() const { return sdlWindow->getWindowScalingSize(); }
	inline std::vector<std::shared_ptr<CircuitViewWidget>> getCircuitViewWidgets() { return circuitViewWidgets; };
	inline std::shared_ptr<CircuitViewWidget> getCircuitViewWidget(unsigned int i) { return circuitViewWidgets[i]; };
	inline std::shared_ptr<CircuitViewWidget> getActiveCircuitViewWidget() { return activeCircuitViewWidget; };

	// void addCircuitViewWidget() // once we can change element that it is attached to
	void createCircuitViewWidget(Rml::Element* element);

	void saveCircuit(circuit_id_t id, bool saveAs);
	void loadCircuit();
	void exportProject();
	void addPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options);
	void savePopUp(const std::string& circuitUUID);
	void saveAsPopUp(const std::string& circuitUUID);

	void setGlobalCssProperty(const std::string& property, const std::string& value);

private:
	void createPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options);
	void offsetUiScale(double delta);
	void applyUiScale(float scale);

	WindowId windowId;
	Environment* environment;

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

	std::vector<std::pair<std::string,const std::vector<std::pair<std::string, std::function<void()>>>>> popUpsToAdd;

	// circuit view widget
	std::shared_ptr<CircuitViewWidget> activeCircuitViewWidget;
	std::vector<std::shared_ptr<CircuitViewWidget>> circuitViewWidgets;

	// rmlui and sdl
	Rml::Context* rmlContext;
	Rml::ElementDocument* rmlDocument;
	std::shared_ptr<SdlWindow> sdlWindow;
};

#endif /* window_h */
