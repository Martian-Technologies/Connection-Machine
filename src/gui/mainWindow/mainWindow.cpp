#include "mainWindow.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <SDL3/SDL_video.h>

#include "gpu/mainRenderer.h"

#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "gui/rml/rmlRenderInterface.h"
#include "gui/rml/rmlSystemInterface.h"

#include "app.h"

#include "backend/settings/settings.h"
#include "computerAPI/directoryManager.h"
#include "environment/environment.h"

MainWindow::MainWindow(Environment& environment) :
	sdlWindow(App::get().registerWindow("Connection Machine")), environment(environment), toolManagerManager(environment), popUpManager(*this) {
	sdlWindow->setRecieveEventFunction(std::bind(&MainWindow::recieveEvent, this, std::placeholders::_1));
	sdlWindow->setRenderFunction(std::bind(&MainWindow::updateRml, this));

	windowId = MainRenderer::get().registerWindow(sdlWindow.get());

	// create rmlUI context
	rmlContext = Rml::CreateContext("mainWindow_" + std::to_string(windowId), Rml::Vector2i(sdlWindow->getSize().first, sdlWindow->getSize().second)); // ptr managed by rmlUi (I think)

	// create rmlUI document
	rmlDocument = rmlContext->LoadDocument(DirectoryManager::getResourceDirectory().generic_string() + "/gui/mainWindow/mainWindow.rml");

	// get widget for circuit view
	Rml::Element* circuitViewWidgetElement = rmlDocument->GetElementById("circuit-view-rendering-area");
	createCircuitViewWidget(circuitViewWidgetElement);

	// eval menutree
	Rml::Element* evalTreeParent = rmlDocument->GetElementById("eval-tree");
	evalWindow.emplace(
		environment.getBackend().getEvaluatorManager(),
		environment.getBackend().getCircuitManager(),
		*this,
		environment.getBackend().getDataUpdateEventManager(),
		rmlDocument,
		evalTreeParent
	);

	//  blocks/tools menutree
	selectorWindow.emplace(
		environment.getBackend().getBlockDataManager(),
		environment.getBackend().getDataUpdateEventManager(),
		environment.getBackend().getCircuitManager().getProceduralCircuitManager(),
		toolManagerManager,
		rmlDocument
	);

	Rml::Element* blockCreationMenu = rmlDocument->GetElementById("block-creation-form");
	blockCreationWindow.emplace(environment.getBackend().getCircuitManager(), environment, *this, environment.getBackend().getDataUpdateEventManager(), toolManagerManager, rmlDocument, blockCreationMenu);

	simControlsManager.emplace(rmlDocument, getCircuitViewWidget(0), environment.getBackend().getDataUpdateEventManager());

	settingsWindow.emplace(rmlDocument);

	menuBar.emplace(rmlDocument, &settingsWindow.value(), this);

	cornerLog.emplace(rmlDocument);

	// keybind handling
	rmlDocument->AddEventListener(Rml::EventId::Keydown, &keybindHandler);
	keybindHandler.addListener("Keybinds/Editing/Paste", [this]() { toolManagerManager.setTool("paste tool"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/State Changer", [this]() { toolManagerManager.setTool("state changer"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/Connection", [this]() { toolManagerManager.setTool("connection"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/Move", [this]() { toolManagerManager.setTool("move"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/Mode Changer", [this]() { toolManagerManager.setTool("mode changer"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/Placement", [this]() { toolManagerManager.setTool("placement"); });
	keybindHandler.addListener("Keybinds/Editing/Tools/Selection Maker", [this]() { toolManagerManager.setTool("selection maker"); });
	keybindHandler.addListener("Keybinds/Window/Toggle Fullscreen", [this]() { sdlWindow->toggleBorderlessFullscreen(); });
	keybindHandler.addListener("Keybinds/Window/Increase UI Scale", [this]() { offsetUiScale(kUiScaleStep); });
	keybindHandler.addListener("Keybinds/Window/Decrease UI Scale", [this]() { offsetUiScale(-kUiScaleStep); });
	keybindHandler.addListener("Keybinds/Window/Reset UI Scale", [this]() { applyUiScale(1.0f); });

	const double* initialUiScale = Settings::get<SettingType::DECIMAL>("Appearance/UI Scale");
	applyUiScale(initialUiScale ? static_cast<float>(*initialUiScale) : 1.0f);
	Settings::registerListener<SettingType::DECIMAL>("Appearance/UI Scale", [this](const double& value) { applyUiScale(static_cast<float>(value)); });

	// show rmlUi document
	rmlDocument->Show();

	Settings::registerListener<SettingType::FILE_PATH>("Appearance/Font", [this](const std::string& fontFilePath) {
		// Rml::LoadFontFace(fontFilePath);
		logInfo("loaded, {}", "", fontFilePath);
	});

}

MainWindow::~MainWindow() {
	App::get().deregisterWindow(sdlWindow);
	Rml::RemoveContext(rmlContext->GetName());
	MainRenderer::get().deregisterWindow(windowId);
	rmlContext = nullptr;
}

bool MainWindow::recieveEvent(SDL_Event& event) {
	if (event.type == SDL_EVENT_KEYMAP_CHANGED) {
		if (settingsWindow) {
			settingsWindow->reloadContent();
		}
	}

	// check if we want this event
	if (!sdlWindow->isThisMyEvent(event)) return event.type == SDL_EVENT_KEYMAP_CHANGED;

	if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
		if (App::get().closeMainWindow(this)) {
			App::get().deregisterWindow(sdlWindow.get());
		}
		return true;
	}

	// send event to RML
	RmlSDL::InputEventHandler(rmlContext, sdlWindow->getHandle(), event, getSdlWindowScalingSize());

	if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
		applyUiScale(uiScale);
	}

	// let renderer know we if resized the window
	if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		MainRenderer::get().resizeWindow(windowId, { event.window.data1, event.window.data2 });
		rmlContext->Update();
		for (auto circuitViewWidget : circuitViewWidgets) {
			circuitViewWidget->handleResize();
		}
	}

	return true;
}

void MainWindow::updateRml() {
	RmlRenderInterface* rmlRenderInterface = dynamic_cast<RmlRenderInterface*>(Rml::GetRenderInterface());
	if (rmlRenderInterface) {
		rmlContext->Update();
		rmlRenderInterface->setWindowToRenderOn(windowId);
		MainRenderer::get().prepareForRmlRender(windowId);
		rmlContext->Render();
		MainRenderer::get().endRmlRender(windowId);
	}
	// update circuit view widget UI components like TPS display
	for (auto& circuitViewWidget : circuitViewWidgets) {
		circuitViewWidget->updateTps();
	}
	cornerLog->updateMessages();
}

void MainWindow::createCircuitViewWidget(Rml::Element* element) {
	circuitViewWidgets.push_back(std::make_shared<CircuitViewWidget>(environment, rmlDocument, *this, windowId, element));
	toolManagerManager.addCircuitView(circuitViewWidgets.back()->getCircuitView());
	activeCircuitViewWidget = circuitViewWidgets.back(); // if it is created, it should be used
}

void MainWindow::offsetUiScale(double delta) {
	const double* storedScale = Settings::get<SettingType::DECIMAL>("Appearance/UI Scale");
	const double currentScale = storedScale ? *storedScale : static_cast<double>(uiScale);
	const double targetScale = std::clamp(currentScale + delta, kUiScaleMin, kUiScaleMax);
	if (std::abs(targetScale - currentScale) < 1e-6) {
		applyUiScale(static_cast<float>(targetScale));
		return;
	}
	if (!Settings::set<SettingType::DECIMAL>("Appearance/UI Scale", targetScale)) {
		applyUiScale(static_cast<float>(targetScale));
	}
}

void MainWindow::applyUiScale(float scale) {
	const float clamped = std::clamp(scale, static_cast<float>(kUiScaleMin), static_cast<float>(kUiScaleMax));
	if (std::abs(static_cast<double>(clamped) - static_cast<double>(scale)) > 1e-6 && !uiScaleSettingUpdateInProgress) {
		const double* storedScale = Settings::get<SettingType::DECIMAL>("Appearance/UI Scale");
		if (!storedScale || std::abs(*storedScale - clamped) > 1e-6) {
			uiScaleSettingUpdateInProgress = true;
			Settings::set<SettingType::DECIMAL>("Appearance/UI Scale", clamped);
			uiScaleSettingUpdateInProgress = false;
		}
	}
	uiScale = clamped;
	if (!rmlContext) return;
	float displayScale = 1.0f;
	if (sdlWindow) {
		displayScale = SDL_GetWindowDisplayScale(sdlWindow->getHandle());
		if (!(displayScale > 0.0f)) {
			displayScale = 1.0f;
		}
	}
	rmlContext->SetDensityIndependentPixelRatio(displayScale * uiScale);

	if (settingsWindow) {
		settingsWindow->reloadContent();
	}

	rmlContext->Update();
	for (auto circuitViewWidget : circuitViewWidgets) {
		circuitViewWidget->handleResize();
	}
}

void setGlobalCssPropertyRec(Rml::Element* element, const std::string& property, const std::string& value) {
	if (!element) return;

	element->SetProperty(property, value);
	for (int i = 0; i < element->GetNumChildren(); i++) {
		setGlobalCssPropertyRec(element->GetChild(i), property, value);
	}
}

void MainWindow::setGlobalCssProperty(const std::string& property, const std::string& value) {
	logInfo("Setting {} to {}", "", property, value);
	setGlobalCssPropertyRec(rmlDocument, property, value);
}
