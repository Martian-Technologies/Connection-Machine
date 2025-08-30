#ifndef window_h
#define window_h

#include <RmlUi/Core.h>
#include <SDL3/SDL_events.h>

#include "gui/sdl/sdlWindow.h"

#include "computerAPI/circuits/circuitFileManager.h"
#include "backend/backend.h"

#include "tools/toolManagerManager.h"
#include "sideBar/icEditor/blockCreationWindow.h"
#include "circuitView/simControlsManager.h"
#include "circuitView/circuitViewWidget.h"
#include "sideBar/selector/selectorWindow.h"
#include "sideBar/simulation/evalWindow.h"


class MainWindow {
public:
	MainWindow(Backend* backend, CircuitFileManager* circuitFileManager);
	~MainWindow();

	// no copy
	MainWindow(const MainWindow&) = delete;
	MainWindow& operator=(const MainWindow&) = delete;

public:
	bool recieveEvent(SDL_Event& event);
	void updateRml();

	inline SDL_Window* getSdlWindow() { return sdlWindow.getHandle(); };
	inline float getSdlWindowScalingSize() const { return sdlWindow.getWindowScalingSize(); }
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

	WindowId windowId;
	Backend* backend;
	KeybindHandler keybindHandler;
	CircuitFileManager* circuitFileManager;
	SdlWindow sdlWindow;
	ToolManagerManager toolManagerManager;
	std::optional<SelectorWindow> selectorWindow;
	std::optional<EvalWindow> evalWindow;
	std::optional<BlockCreationWindow> blockCreationWindow;
	std::optional<SimControlsManager> simControlsManager;

	std::shared_ptr<CircuitViewWidget> activeCircuitViewWidget;
	std::vector<std::shared_ptr<CircuitViewWidget>> circuitViewWidgets;

	Rml::Context* rmlContext;
	Rml::ElementDocument* rmlDocument;

	std::vector<std::pair<std::string,const std::vector<std::pair<std::string, std::function<void()>>>>> popUpsToAdd;
};

#endif /* window_h */
