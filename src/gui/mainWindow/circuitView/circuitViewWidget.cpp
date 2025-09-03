#include "circuitViewWidget.h"

#include <SDL3/SDL.h>

#include "backend/settings/settings.h"
#include "backend/backend.h"

#include "gui/viewportManager/circuitView/events/customEvents.h"
#include "gui/viewportManager/circuitView/circuitView.h"
#include "gui/mainWindow/mainWindow.h"
#include "gui/helper/keybindHelpers.h"
#include "gui/helper/eventPasser.h"

#include "environment/environment.h"

void LoadCallback(void* userData, const char* const* filePaths, int filter) {
	CircuitViewWidget* circuitViewWidget = (CircuitViewWidget*)userData;
	if (filePaths && filePaths[0]) {
		std::string filePath = filePaths[0];
		std::vector<circuit_id_t> ids = circuitViewWidget->getFileManager()->loadFromFile(filePath);
		if (ids.empty()) {
			// logError("Failed to load circuit file."); // Not a error! It is valid to load 0 circuits.
			return;
		}
		circuit_id_t id = ids.back();
		circuitViewWidget->getCircuitView()->setCircuit(circuitViewWidget->getCircuitView()->getBackend(), id);
		// circuitViewWidget->getCircuitView()->getBackend()->linkCircuitViewWithCircuit(circuitViewWidget->getCircuitView(), id);
		for (auto& iter : circuitViewWidget->getCircuitView()->getBackend()->getEvaluatorManager().getEvaluators()) {
			if (iter.second->getCircuitId(Address()) == id) {
				circuitViewWidget->getCircuitView()->setEvaluator(circuitViewWidget->getCircuitView()->getBackend(), iter.first);
				// circuitViewWidget->getCircuitView()->getBackend()->linkCircuitViewWithEvaluator(circuitViewWidget->getCircuitView(), iter.first, Address());
				return;
			}
		}
	} else {
		logInfo("File dialog canceled.");
	}
}

CircuitViewWidget::CircuitViewWidget(
	Environment* environment,
	Rml::ElementDocument* document,
	MainWindow* mainWindow,
	WindowId windowId,
	Rml::Element* element) :
	fileManager(&environment->getCircuitFileManager()), document(document), mainWindow(mainWindow), windowId(windowId), element(element) {
	// create circuitView
	int w = this->element->GetClientWidth();
	int h = this->element->GetClientHeight();
	int x = this->element->GetAbsoluteLeft() + this->element->GetClientLeft();
	int y = this->element->GetAbsoluteTop() + this->element->GetClientTop();
	viewportId = MainRenderer::get().registerViewport(windowId, {x, y}, {w, h});
	circuitView = std::make_unique<CircuitView>(environment, viewportId);

	circuitView->getEventRegister().registerFunction("status bar changed", [this](const Event* event) -> bool {
		auto eventData = event->cast2<std::string>();
		if (eventData) setStatusBar(eventData->get());
		return false;
	});

	Settings::registerListener<SettingType::BOOL>("Keybinds/Camera/Scroll Panning", [this](const bool& enabled) {
		mouseControls = !enabled;
	});

	// set initial view
	element->AddEventListener(Rml::EventId::Resize, new EventPasser([this](Rml::Event&){handleResize();}));
	handleResize();

	// create keybind shortcuts and connect them
	document->AddEventListener(Rml::EventId::Keydown, &keybindHandler);
	keybindHandler.addListener(
		"Keybinds/Editing/Undo",
		[this]() { if (circuitView->getCircuit()) circuitView->getCircuit()->undo(); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Redo",
		[this]() { if (circuitView->getCircuit()) circuitView->getCircuit()->redo(); }
	);
	keybindHandler.addListener(
		"Keybinds/File/Save",
		[this]() { save(); }
	);
	keybindHandler.addListener(
		"Keybinds/File/Save As",
		[this]() { asSave(); }
	);
	keybindHandler.addListener(
		"Keybinds/File/Open",
		[this]() { load(); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Copy",
		[this]() { circuitView->getEventRegister().doEvent(Event("Copy")); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Rotate CCW",
		[this]() { circuitView->getEventRegister().doEvent(Event("Tool Rotate Block CCW")); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Rotate CW",
		[this]() { circuitView->getEventRegister().doEvent(Event("Tool Rotate Block CW")); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Confirm",
		[this]() { circuitView->getEventRegister().doEvent(Event("Confirm")); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tool Invert Mode",
		[this]() { circuitView->getEventRegister().doEvent(Event("Tool Invert Mode")); }
	);
	keybindHandler.addListener(
		"Keybinds/File/New",
		[this]() { newCircuit(); }
	);

	Rml::Element* root = document->GetElementById("main-container");
	root->AddEventListener(Rml::EventId::Mouseup, new EventPasser(
		[this](Rml::Event& event) {
			int button = event.GetParameter<int>("button", 0);
			if (button == 0) { // left
				circuitView->getEventRegister().doEvent(PositionEvent("View Dettach Anchor", circuitView->getViewManager().getPointerPosition()));
				circuitView->getEventRegister().doEvent(PositionEvent("tool primary deactivate", circuitView->getViewManager().getPointerPosition()));
			} else if (button == 1) { // right
				circuitView->getEventRegister().doEvent(PositionEvent("tool secondary deactivate", circuitView->getViewManager().getPointerPosition()));
			}
		}
	));

	element->AddEventListener(Rml::EventId::Mousemove, new EventPasser(
		[this](Rml::Event& event) {
			SDL_FPoint point(event.GetParameter<int>("mouse_x", 0), event.GetParameter<int>("mouse_y", 0));
			if (insideWindow(point)) { // inside the widget
				Vec2 viewPos = pixelsToView(point);
				circuitView->getEventRegister().doEvent(ViewPositionEvent("Pointer Move On View", viewPos));
			}
		}
	));

	element->AddEventListener(Rml::EventId::Mousedown, new EventPasser(
		[this](Rml::Event& event) {
			int button = event.GetParameter<int>("button", 0);
			if (button == 0) { // left
				if (event.GetParameter<int>("alt_key", 0)) {
					if (circuitView->getEventRegister().doEvent(PositionEvent("View Attach Anchor", circuitView->getViewManager().getPointerPosition()))) { event.StopPropagation(); return; }
				}
				if (circuitView->getEventRegister().doEvent(PositionEvent("Tool Primary Activate", circuitView->getViewManager().getPointerPosition()))) event.StopPropagation();
			} else if (button == 1) { // right
				if (circuitView->getEventRegister().doEvent(PositionEvent("Tool Secondary Activate", circuitView->getViewManager().getPointerPosition()))) event.StopPropagation();
			}
		}
	));

	element->AddEventListener(Rml::EventId::Mouseover, new EventPasser(
		[this](Rml::Event& event) {
			// // grab focus so key inputs work without clicking
			// setFocus(Qt::MouseFocusReason);
			this->document->Focus();
			SDL_FPoint point(event.GetParameter<int>("mouse_x", 0), event.GetParameter<int>("mouse_y", 0));
			Vec2 viewPos = pixelsToView(point);
			if (viewPos.x < 0 || viewPos.y < 0 || viewPos.x > 1 || viewPos.y > 1) return;
			circuitView->getEventRegister().doEvent(PositionEvent("pointer enter view", circuitView->getViewManager().viewToGrid(viewPos)));
		}
	));

	element->AddEventListener(Rml::EventId::Mouseout, new EventPasser(
		[this](Rml::Event& event) {
			SDL_FPoint point(event.GetParameter<int>("mouse_x", 0), event.GetParameter<int>("mouse_y", 0));
			Vec2 viewPos = pixelsToView(point);
			if (viewPos.x >= 0 && viewPos.y >= 0 && viewPos.x <= 1 && viewPos.y <= 1) return;
			circuitView->getEventRegister().doEvent(PositionEvent("pointer exit view", circuitView->getViewManager().viewToGrid(viewPos)));
		}
	));

	element->AddEventListener(Rml::EventId::Mousescroll, new EventPasser(
		[this](Rml::Event& event) {
			SDL_FPoint delta(event.GetParameter<float>("wheel_delta_x", 0) * 12, event.GetParameter<float>("wheel_delta_y", 0) * -12);
			// logInfo("{}, {}", "", delta.x, delta.y);
			if (mouseControls) {
				if (circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(delta.y) / 150.f))) event.StopPropagation();
			} else {
				const Keybind* keybind = Settings::get<SettingType::KEYBIND>("Keybinds/Camera/Zoom");
				if (keybind && makeKeybind(event) == *keybind) {
					// do zoom
					if (circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(delta.y) / 150.f))) event.StopPropagation();
				} else {
					if (circuitView->getEventRegister().doEvent(DeltaXYEvent(
						"view pan",
						delta.x / getPixelsWidth() * circuitView->getViewManager().getViewWidth(),
						delta.y / getPixelsHeight() * circuitView->getViewManager().getViewHeight()
					))) event.StopPropagation();
				}
			}
		}
	));

	element->AddEventListener("pinch", new EventPasser(
		[this](Rml::Event& event) {
			float delta = event.GetParameter<float>("delta", 0);
			logInfo(delta);
			if (circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(delta) * 50))) event.StopPropagation();
			// if (mouseControls) {
			// } else {
			// 	if (event.GetParameter<int>("shift_key", 0)) {
			// 		// do zoom
			// 		if (circuitView->getEventRegister().doEvent(DeltaEvent("view zoom", (float)(delta.y) / 100.f))) event.StopPropagation();
			// 	} else {
			// 		if (circuitView->getEventRegister().doEvent(DeltaXYEvent(
			// 			"view pan",
			// 			delta.x / getPixelsWidth() * circuitView->getViewManager().getViewWidth(),
			// 			delta.y / getPixelsHeight() * circuitView->getViewManager().getViewHeight()
			// 		))) event.StopPropagation();
			// 	}
			// }
		}
	));
}


void CircuitViewWidget::render() {
	Evaluator* evaluator = circuitView->getEvaluator();
	std::string tpsText = "Real tps: N/A";
	if (evaluator) {
		tpsText = "Real tps: " + fmt::format("{:.2f}", evaluator->getRealTickrate());
	}
	Rml::Element* realTpsDisplay = document->GetElementById("real-tps-display");
	if (realTpsDisplay) {
		Rml::Element* realTpsDisplayElement = realTpsDisplay->GetChild(0);
		if (!realTpsDisplayElement)
			realTpsDisplayElement = realTpsDisplay->AppendChild(std::move(realTpsDisplay->GetOwnerDocument()->CreateTextNode("")));
		Rml::ElementText* realTpsDisplayText = rmlui_dynamic_cast<Rml::ElementText*>(realTpsDisplayElement);
		if (realTpsDisplayText && realTpsDisplayText->GetText() != tpsText) {
			realTpsDisplayText->SetText(tpsText);
		}
	}
}

void CircuitViewWidget::setSimState(bool state) {
	if (circuitView->getEvaluator())
		circuitView->getEvaluator()->setPause(!state);
}

void CircuitViewWidget::simUseSpeed(bool state) {
	if (circuitView->getEvaluator())
		circuitView->getEvaluator()->setUseTickrate(state);
}

void CircuitViewWidget::setSimSpeed(double speed) {
	if (circuitView->getEvaluator())
		circuitView->getEvaluator()->setTickrate(speed);
}

void CircuitViewWidget::newCircuit() {
	if (circuitView->getBackend() == nullptr) {
		logError("Can't make circuit when no backend is set", "CircuitViewWidget");
		return;
	}
	circuit_id_t id = circuitView->getBackend()->createCircuit();
	if (id == 0) return; // other logs shoud happen before this
	circuitView->setCircuit(circuitView->getBackend(), id);
	// tmp get eval with this circuit id because circuit manager makes a eval for loaded circuits
	for (auto& iter : circuitView->getBackend()->getEvaluatorManager().getEvaluators()) {
		if (iter.second->getCircuitId(Address()) == id) {
			circuitView->setEvaluator(circuitView->getBackend(), iter.second);
			// circuitView->getBackend()->linkCircuitViewWithEvaluator(circuitView.get(), iter.first, Address());
			return;
		}
	}
}

void CircuitViewWidget::setStatusBar(const std::string& text) {
	Rml::Element* statusBar = element->GetElementById("status-bar");
	statusBar->SetInnerRML(text);
}

// save current circuit view widget we are viewing. Right now only works if it is the only widget in application.
// Called via Ctrl-S keybind
void CircuitViewWidget::save() {
	if (circuitView->getCircuit()) mainWindow->savePopUp(circuitView->getCircuit()->getUUID());
}

void CircuitViewWidget::asSave() {
	if (circuitView->getCircuit()) mainWindow->saveAsPopUp(circuitView->getCircuit()->getUUID());
}

// for drag and drop load directly onto this circuit view widget
void CircuitViewWidget::load() {
	if (!fileManager) return;

	static const SDL_DialogFileFilter filters[] = {
		{ "Circuit Files",  ".cir" },
		{ "Circuit Files",  ".blif" },
		{ "Circuit Files",  ".wasm" },
	};

	SDL_ShowOpenFileDialog(LoadCallback, this, nullptr, filters, 3, nullptr, true);
}

void CircuitViewWidget::handleResize() {
	int w = this->element->GetClientWidth();
	int h = this->element->GetClientHeight();
	int x = this->element->GetAbsoluteLeft() + this->element->GetClientLeft();
	int y = this->element->GetAbsoluteTop() + this->element->GetClientTop();

	circuitView->getViewManager().setAspectRatio((float)w / (float)h);
	MainRenderer::get().moveViewport(viewportId, windowId, {x, y}, {w, h});
}

inline Vec2 CircuitViewWidget::pixelsToView(const SDL_FPoint& point) const {
	return Vec2((point.x - getPixelsXPos()) / getPixelsWidth(), (point.y - getPixelsYPos()) / getPixelsHeight());
}

inline bool CircuitViewWidget::insideWindow(const SDL_FPoint& point) const {
	int x = point.x - getPixelsXPos();
	int y = point.y - getPixelsYPos();
	return x >= 0 && y >= 0 && x < getPixelsWidth() && y < getPixelsHeight();
}

inline float CircuitViewWidget::getPixelsWidth() const {
	return element->GetClientWidth();
}

inline float CircuitViewWidget::getPixelsHeight() const {
	return element->GetClientHeight();
}

inline float CircuitViewWidget::getPixelsXPos() const {
	return element->GetAbsoluteLeft() + element->GetClientLeft();
}

inline float CircuitViewWidget::getPixelsYPos() const {
	return element->GetAbsoluteTop() + element->GetClientTop();
}
