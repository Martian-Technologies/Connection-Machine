#ifndef circuitViewWidget_h
#define circuitViewWidget_h

#include <RmlUi/Core.h>

#include "computerAPI/circuits/circuitFileManager.h"

#include "gui/viewportManager/circuitView/circuitView.h"
#include "gui/helper/keybindHandler.h"

#include "util/vec2.h"

class MainWindow;

class CircuitViewWidget {
public:
	CircuitViewWidget(Environment* environment, Rml::ElementDocument* document, MainWindow* mainWindow, WindowId windowId, Rml::Element* element);
	~CircuitViewWidget() { element->RemoveEventListener("keydown", &keybindHandler); }

	// setup
	inline CircuitView* getCircuitView() { return circuitView.get(); }
	inline CircuitFileManager* getFileManager() { return fileManager; }
	void setSimState(bool state);
	void simUseSpeed(bool state);
	void setSimSpeed(double speed);
	void setStatusBar(const std::string& text = "");

	void render();
	void handleResize();

	void newCircuit();
	void load();
	void save();
	void asSave();

private:
	// utility functions
	inline Vec2 pixelsToView(const SDL_FPoint& point) const;
	inline bool insideWindow(const SDL_FPoint& point) const;
	inline float getPixelsWidth() const;
	inline float getPixelsHeight() const;
	inline float getPixelsXPos() const;
	inline float getPixelsYPos() const;

	WindowId windowId;
	ViewportId viewportId;
	std::unique_ptr<CircuitView> circuitView;
	CircuitFileManager* fileManager;
	Rml::ElementDocument* document;
	Rml::Element* element;
	MainWindow* mainWindow;
	KeybindHandler keybindHandler;

	// settings (temp)
	bool mouseControls = false;
};

#endif /* circuitViewWidget_h */
