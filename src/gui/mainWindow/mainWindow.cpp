#include "mainWindow.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include "gpu/mainRenderer.h"

#include "gui/mainWindow/circuitView/circuitViewWidget.h"
#include "gui/mainWindow/menuBar/menuBar.h"
#include "gui/rml/rmlSystemInterface.h"
#include "gui/rml/rmlRenderInterface.h"
#include "gui/helper/eventPasser.h"

#include "settingsWindow/settingsWindow.h"
#include "computerAPI/directoryManager.h"
#include "appInstance.h"

MainWindow::MainWindow(AppInstance* appInstance) :
	sdlWindow("Connection Machine"), appInstance(appInstance), toolManagerManager(appInstance->getBackend().getDataUpdateEventManager()) {

	windowId = MainRenderer::get().registerWindow(&sdlWindow);

	// create rmlUI context
	rmlContext = Rml::CreateContext("main", Rml::Vector2i(sdlWindow.getSize().first, sdlWindow.getSize().second)); // ptr managed by rmlUi (I think)

	// create rmlUI document
	rmlDocument = rmlContext->LoadDocument(DirectoryManager::getResourceDirectory().generic_string() + "/gui/mainWindow/mainWindow.rml");

	// Rml::Debugger::Initialise(rmlContext);
	// Rml::Debugger::SetVisible(true);

	// get widget for circuit view
	Rml::Element* circuitViewWidgetElement = rmlDocument->GetElementById("circuit-view-rendering-area");
	createCircuitViewWidget(circuitViewWidgetElement);

	// eval menutree
	Rml::Element* evalTreeParent = rmlDocument->GetElementById("eval-tree");
	evalWindow.emplace(&(appInstance->getBackend().getEvaluatorManager()), &(appInstance->getBackend().getCircuitManager()), this, appInstance->getBackend().getDataUpdateEventManager(), rmlDocument, evalTreeParent);

	//  blocks/tools menutree
	selectorWindow.emplace(
		appInstance->getBackend().getBlockDataManager(),
		appInstance->getBackend().getDataUpdateEventManager(),
		appInstance->getBackend().getCircuitManager().getProceduralCircuitManager(),
		&toolManagerManager,
		rmlDocument
	);

	Rml::Element* blockCreationMenu = rmlDocument->GetElementById("block-creation-form");
	blockCreationWindow.emplace(&(appInstance->getBackend().getCircuitManager()), this, appInstance->getBackend().getDataUpdateEventManager(), &toolManagerManager, rmlDocument, blockCreationMenu);

	simControlsManager.emplace(rmlDocument, getCircuitViewWidget(0), appInstance->getBackend().getDataUpdateEventManager());

	SettingsWindow* settingsWindow = new SettingsWindow(rmlDocument);

	MenuBar* menuBar = new MenuBar(rmlDocument, settingsWindow, this);

	// keybind handling
	rmlDocument->AddEventListener(Rml::EventId::Keydown, &keybindHandler);
	keybindHandler.addListener(
		"Keybinds/Editing/Paste",
		[this]() { toolManagerManager.setTool("paste tool"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/State Changer",
		[this]() { toolManagerManager.setTool("state changer"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/Connection",
		[this]() { toolManagerManager.setTool("connection"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/Move",
		[this]() { toolManagerManager.setTool("move"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/Mode Changer",
		[this]() { toolManagerManager.setTool("mode changer"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/Placement",
		[this]() { toolManagerManager.setTool("placement"); }
	);
	keybindHandler.addListener(
		"Keybinds/Editing/Tools/Selection Maker",
		[this]() { toolManagerManager.setTool("selection maker"); }
	);

	// show rmlUi document
	rmlDocument->Show();

	// example pop up
	// addPopUp("this is a test", {
	// 	std::make_pair<std::string, std::function<void()>>("A", [](){logInfo("A");}),
	// 	std::make_pair<std::string, std::function<void()>>("B", [](){logInfo("B");}),
	// 	std::make_pair<std::string, std::function<void()>>("C", [](){logInfo("C");})
	// });

	Settings::registerListener<SettingType::FILE_PATH>("Appearance/Font", [this](const std::string& fontFilePath) {
		// Rml::LoadFontFace(fontFilePath);
		logInfo("loaded, {}", "", fontFilePath);
	});
}

MainWindow::~MainWindow() {
	Rml::RemoveContext(rmlContext->GetName());
	MainRenderer::get().deregisterWindow(windowId);
	rmlContext = nullptr;
}

bool MainWindow::recieveEvent(SDL_Event& event) {
	// check if we want this event
	if (sdlWindow.isThisMyEvent(event)) {
		if (event.type == SDL_EVENT_DROP_FILE) {
			std::string file = event.drop.data;
			std::cout << file << "\n";
			std::vector<circuit_id_t> ids = getActiveCircuitViewWidget()->getFileManager()->loadFromFile(file);
			if (ids.empty()) {
				// logError("Error", "Failed to load circuit file."); // Not a error! It is valid to load 0 circuits.
			} else {
				circuit_id_t id = ids.back();
				if (id == 0) {
					logError("Error", "Failed to load circuit file.");
				} else {
					getActiveCircuitViewWidget()->getCircuitView()->setCircuit(&appInstance->getBackend(), id);
					// circuitViewWidget->getCircuitView()->getBackend()->linkCircuitViewWithCircuit(circuitViewWidget->getCircuitView(), id);
					for (auto& iter : appInstance->getBackend().getEvaluatorManager().getEvaluators()) {
						if (iter.second->getCircuitId(Address()) == id) {
							getActiveCircuitViewWidget()->getCircuitView()->setEvaluator(&appInstance->getBackend(), iter.first);
							// circuitViewWidget->getCircuitView()->getBackend()->linkCircuitViewWithEvaluator(circuitViewWidget->getCircuitView(), iter.first, Address());
						}
					}
				}
			}
		}

		// send event to RML
		RmlSDL::InputEventHandler(rmlContext, sdlWindow.getHandle(), event, getSdlWindowScalingSize());

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

	return false;
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
}

void MainWindow::createCircuitViewWidget(Rml::Element* element) {
	circuitViewWidgets.push_back(std::make_shared<CircuitViewWidget>(&appInstance->getCircuitFileManager(), rmlDocument, this, windowId, element));
	circuitViewWidgets.back()->getCircuitView()->setBackend(&appInstance->getBackend());
	toolManagerManager.addCircuitView(circuitViewWidgets.back()->getCircuitView());
	activeCircuitViewWidget = circuitViewWidgets.back(); // if it is created, it should be used
}

void MainWindow::addPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options) {
	if (popUpsToAdd.empty()) {
		createPopUp(message, options);
	} else {
		popUpsToAdd.emplace_back(message, options);
	}
}

void MainWindow::createPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options) {
	rmlDocument->GetElementById("pop-up-overlay")->SetClass("invisible", false);
	rmlDocument->GetElementById("pop-up-text")->SetInnerRML(message);
	Rml::Element* actionsElement = rmlDocument->GetElementById("pop-up-actions");
	while (actionsElement->HasChildNodes()) { actionsElement->RemoveChild(actionsElement->GetChild(0)); }
	for (const auto& option : options) {
		Rml::ElementPtr setPositionButton = rmlDocument->CreateElement("button");
		setPositionButton->AppendChild(std::move(rmlDocument->CreateTextNode(option.first)));
		setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser(
			[this, func = option.second](Rml::Event& event) {
				rmlDocument->GetElementById("pop-up-overlay")->SetClass("invisible", true);
				func();
			}
		));
		setPositionButton->SetClass("pop-up-action", true);
		actionsElement->AppendChild(std::move(setPositionButton));
	}
}

void SaveCallback(void* userData, const char* const* filePaths, int filter) {
	std::pair<CircuitFileManager*, std::string>* data = (std::pair<CircuitFileManager*, std::string>*)userData;
	if (filePaths && filePaths[0]) {
		std::string filePath = filePaths[0];
		if (data->first->getSavePath(data->second) != nullptr)
			logWarning("This circuit " + data->second + " will be saved with a new UUID");
		data->first->saveToFile(filePath, data->second);
	} else {
		std::cout << "File dialog canceled." << std::endl;
	}
	delete data;
}

void MainWindow::savePopUp(const std::string& circuitUUID) {
	if (!appInstance->getCircuitFileManager().save(circuitUUID)) {
		// if failed to save the circuit with out a path
		static const SDL_DialogFileFilter filters[] = {
			{ "Circuit Files",  ".cir" }
		};
		std::pair<CircuitFileManager*, std::string>* data = new std::pair<CircuitFileManager*, std::string>(&appInstance->getCircuitFileManager(), circuitUUID);
		SDL_ShowSaveFileDialog(SaveCallback, data, sdlWindow.getHandle(), filters, 1, nullptr);
	}
}

void MainWindow::saveAsPopUp(const std::string& circuitUUID) {
	static const SDL_DialogFileFilter filters[] = {
		{ "Circuit Files",  ".cir" }
	};
	std::pair<CircuitFileManager*, std::string>* data = new std::pair<CircuitFileManager*, std::string>(&appInstance->getCircuitFileManager(), circuitUUID);
	SDL_ShowSaveFileDialog(SaveCallback, data, sdlWindow.getHandle(), filters, 1, nullptr);
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
