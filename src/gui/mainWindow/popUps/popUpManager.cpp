#include "popUpManager.h"

#include "app.h"
#include "computerAPI/directoryManager.h"
#include "environment/environment.h"
#include "gui/helper/eventPasser.h"
#include "gui/helper/saveCallback.h"
#include "gui/mainWindow/mainWindow.h"
#include "network/network.h"
#include "util/compression.h"
#include "util/version.h"

#include <RmlUi/Debugger.h>

#include <SDL3/SDL.h>

std::optional<PopUpManager::PopUpWindow> PopUpManager::createPopUp(bool blocking, const std::string& windowName, unsigned int width, unsigned int height) {
	if (blocking) {
		Rml::Element* allPopUps = mainWindow.getRmlDocument()->GetElementById("all-pop-ups");
		allPopUps->SetClass("invisible", false);
		assert(allPopUps);
		Rml::Element* popUpOverlay = allPopUps->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		popUpOverlay->SetClass("pop-up-overlay", true);
		Rml::Element* popUpOverlayInternal = popUpOverlay->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		popUpOverlayInternal->SetClass("pop-up-overlay-internal", true);
		Rml::Element* popUpWindow = popUpOverlayInternal->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		popUpWindow->SetClass("pop-up-window", true);
		popUpWindow->SetClass("pop-up-window-blocked", true);
		popUpWindow->SetClass("bordered", true);
		popUpWindow->SetClass("bg-3", true);
		return PopUpWindow(popUpOverlay->GetContext(), popUpOverlay, [popUpOverlay, allPopUps]() {
			App::get().queForEndOfUpdate([popUpOverlay, allPopUps]() {
				allPopUps->RemoveChild(popUpOverlay);
				if (!allPopUps->HasChildNodes()) allPopUps->SetClass("invisible", true);
			});
		});
	} else {
		SdlWindow* sdlWindow = App::get().registerWindow(windowName, width, height).get();
		WindowId windowId = MainRenderer::get().registerWindow(sdlWindow);
		Rml::Context* rmlContext = Rml::CreateContext("popup_" + std::to_string(windowId), Rml::Vector2i(sdlWindow->getSize().first, sdlWindow->getSize().second));
		if (rmlContext) {
			Rml::ElementDocument* rmlDocument = rmlContext->LoadDocument(DirectoryManager::getResourceDirectory().generic_string() + "/gui/mainWindow/popUpWindow/popUpWindow.rml");
			sdlWindow->setRenderFunction([windowId, rmlContext]() {
				RmlRenderInterface* rmlRenderInterface = dynamic_cast<RmlRenderInterface*>(Rml::GetRenderInterface());
				if (rmlRenderInterface) {
					rmlContext->Update();
					rmlRenderInterface->setWindowToRenderOn(windowId);
					MainRenderer::get().prepareForRmlRender(windowId);
					rmlContext->Render();
					MainRenderer::get().endRmlRender(windowId);
				}
			});
			sdlWindow->setRecieveEventFunction([windowId, rmlContext, sdlWindow](SDL_Event& event) {
				if (sdlWindow->isThisMyEvent(event)) {
					if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
						Rml::RemoveContext(rmlContext->GetName());
						MainRenderer::get().deregisterWindow(windowId);
						App::get().deregisterWindow(sdlWindow);
						return true;
					}

					RmlSDL::InputEventHandler(rmlContext, sdlWindow->getHandle(), event, sdlWindow->getWindowScalingSize());

					// let renderer know we if resized the window
					if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
						MainRenderer::get().resizeWindow(windowId, { event.window.data1, event.window.data2 });
						rmlContext->Update();
					}
					return true;
				}
				return false;
			});
			rmlDocument->Show();
			return PopUpWindow(rmlContext, rmlDocument, [windowId, rmlContext, sdlWindow]() {
				App::get().queForEndOfUpdate([windowId, rmlContext, sdlWindow]() {
					Rml::RemoveContext(rmlContext->GetName());
					MainRenderer::get().deregisterWindow(windowId);
					App::get().deregisterWindow(sdlWindow);
				});
			});
		}
		logError("Failed to create new window for popup :(");
		return std::nullopt;
	}
}

void PopUpManager::addOptionsPopUp(
	const std::string& message,
	const std::optional<std::string>& smallMessage,
	const std::vector<std::pair<std::string, std::function<void()>>>& options,
	bool blocking
) {
	std::optional<PopUpWindow> optionalPopUpWindow = createPopUp(blocking);
	if (!optionalPopUpWindow) return;
	PopUpWindow popUpWindow = std::move(optionalPopUpWindow.value());
	// split message into multiple lines at \n
	Rml::Element* textContainer = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	textContainer->SetClass("pop-up-text-container", true);
	size_t start = 0;
	while (start < message.size()) {
		size_t end = message.find('\n', start);
		if (end == std::string::npos) end = message.size();
		std::string line = message.substr(start, end - start);
		Rml::Element* lineElement = textContainer->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		lineElement->SetInnerRML(line);
		lineElement->SetClass("pop-up-text-line", true);
		start = end + 1;
	}
	if (smallMessage.has_value()) {
		start = 0;
		while (start < smallMessage->size()) {
			size_t end = smallMessage->find('\n', start);
			if (end == std::string::npos) end = smallMessage->size();
			std::string line = smallMessage->substr(start, end - start);
			Rml::Element* lineElement = textContainer->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
			lineElement->SetInnerRML(line);
			lineElement->SetClass("pop-up-text-small-line", true);
			start = end + 1;
		}
	}
	Rml::Element* actionsElement = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("span"));
	actionsElement->SetClass("pop-up-actions", true);
	for (const auto& option : options) {
		Rml::ElementPtr setPositionButton = mainWindow.getRmlDocument()->CreateElement("button");
		setPositionButton->AppendChild(std::move(mainWindow.getRmlDocument()->CreateTextNode(option.first)));
		setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser([popUpWindow, func = option.second](Rml::Event& event) {
			popUpWindow.destroy();
			func();
		}));
		setPositionButton->SetClass("pop-up-action", true);
		actionsElement->AppendChild(std::move(setPositionButton));
	}
}


void PopUpManager::savePopUp(const std::string& circuitUUID) {
	if (mainWindow.getEnvironment().getCircuitFileManager().save(circuitUUID)) {
		mainWindow.log("Circuit was successfully saved.");
	} else {
		// if failed to save the circuit with out a path
		saveAsPopUp(circuitUUID);
	}
}

void PopUpManager::saveAsPopUp(const std::string& circuitUUID) {
	static const SDL_DialogFileFilter filters[] = { { "Circuit Files", "cir" } };
	std::pair<CircuitFileManager*, std::string>* data = new std::pair<CircuitFileManager*, std::string>(&mainWindow.getEnvironment().getCircuitFileManager(), circuitUUID);
	SDL_ShowSaveFileDialog(SaveCallback, data, nullptr, filters, 1, nullptr); // should call mainWindow.log("Circuit was successfully saved.");
}

void set_invisible(Rml::Element* element, bool invisible) {
	if (invisible) {
		element->SetClass("invisible", true);
		for (int i = 0; i < element->GetNumChildren(); ++i) {
			element->GetChild(i)->SetClass("invisible", true);
		}
	} else {
		element->SetClass("invisible", false);
		for (int i = 0; i < element->GetNumChildren(); ++i) {
			element->GetChild(i)->SetClass("invisible", false);
		}
	}
}

void PopUpManager::aboutConnectionMachine() {
	std::optional<PopUpWindow> optionalPopUpWindow = createPopUp(true);
	if (!optionalPopUpWindow) return;
	PopUpWindow popUpWindow = std::move(optionalPopUpWindow.value());

	popUpWindow.getPopUpWindow()->SetAttribute("style", "display: flex; flex-direction: column; width: 480dp; height: 320dp; background-color:#303030;");
	Rml::Element* leftandright = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	leftandright->SetAttribute("style", "display: flex; flex-direction: row; width: 100%; height: 100%;");

	Rml::Element* leftdiv = leftandright->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	leftdiv->SetAttribute("style", "display: flex; flex-direction: column; flex: 1; height: 100%; padding-right: 20dp;");

	Rml::Element* title = leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	title->SetInnerRML("About Connection Machine");
	title->SetAttribute("style", "font-size: 18dp;");
	title->SetClass("popup-title", true);
	title->SetAttribute("style", "font-size: 25px;");

	leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"))->SetAttribute("style", "height: 10dp;");

	Rml::Element* body = leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	body->SetInnerRML("Connection Machine is an open source project aiming to create an application for designing and simulating logic graph systems.");
	body->SetAttribute("style", "padding: 5dp; border-radius: 5dp;");
	body->SetClass("surface-raised", true);

	leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"))->SetAttribute("style", "height: 10dp;");

	Rml::Element* links = leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	links->SetAttribute("style", "padding: 5dp; border-radius: 4dp; display: flex; flex-direction: column;");
	links->SetClass("surface-raised", true);

	Rml::Element* gitHubLink = links->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	gitHubLink->SetInnerRML("GitHub: <p style=\"color: #6969ffff\">github.com/Connection-Machine</p>");
	gitHubLink->AddEventListener(Rml::EventId::Click, new EventPasser([](Rml::Event& event){
		SDL_OpenURL("https://github.com/Martian-Technologies/Connection-Machine");
	}));

	links->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"))->SetAttribute("style", "height: 8dp;");

	Rml::Element* website = links->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	website->SetInnerRML("Website: <p style=\"color: #6969ffff\">connection-machine.com</p>");
	website->AddEventListener(Rml::EventId::Click, new EventPasser([](Rml::Event& event){
		SDL_OpenURL("https://connection-machine.com");
	}));

	leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"))->SetAttribute("style", "flex: 1");

    Rml::Element* rightdiv = leftandright->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
    rightdiv->SetAttribute("style", "display: flex; flex-direction: column; width: 144dp; height: 100%; background-color:#303030;");

    // Add a logo (adjust the src path to your actual asset)
    Rml::Element* logo = rightdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("img"));
    logo->SetAttribute("src", "../../gateIcon.png");
	logo->SetAttribute("style", "margin-left: 15%; margin-right: 15%; margin-bottom: 5%; width: 108dp; height: 108dp;");

    // Add contributor names
    Rml::Element* contributorsTitle = rightdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
    contributorsTitle->SetInnerRML("Contributors:");
    contributorsTitle->SetClass("popup-subtitle", true);

	Rml::Element* rightdivScrollArea = rightdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	rightdivScrollArea->SetAttribute("style", "position: relative; flex-grow: 1; border-radius: 8dp; background-color:#3b3b3b;");
	Rml::Element* rightdivScroll = rightdivScrollArea->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));

	std::vector<std::string> contributors = {
        "Ben Herman", "Nikita Lurye", "Jack Jamison", "Connor Kostiew", "Gregory Lemonnier", "James P", "Sam C", "Alek Krupka",
		"August Bernberg", "Nicholas Ciuica", "Matthew Durcan", "Tiffany Cheng", "Patrick Chen", "Nathan Chen", "Dante L"
    };

	std::string all_contributors = "";
    for (const auto& name : contributors){
		all_contributors = all_contributors + "<p>" + name.c_str() + "</p>";
	}
	rightdivScroll->SetInnerRML(all_contributors);
	rightdivScroll->SetAttribute("style", "position: absolute; top: 0; bottom: 0; width: 100%; overflow-y: scroll; display: flex; flex-direction: column; color: #d1963a; padding: 8dp;");

	Rml::Element* close = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("button"));
    close->SetInnerRML("Close");
    close->SetClass("popup-button", true);
	close->AddEventListener(Rml::EventId::Click, new EventPasser(
	    [popUpWindow](Rml::Event& event) {
	        popUpWindow.destroy();
	    }
	));
}

void PopUpManager::addFeedbackPopup() { // feature request, bug report, feature complaint, feedback
	std::optional<PopUpWindow> optionalPopUpWindow = createPopUp(false);
	if (!optionalPopUpWindow) return;
	PopUpWindow popUpWindow = std::move(optionalPopUpWindow.value());

	Rml::Element* title = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	title->SetInnerRML("We'd love your feedback");
	title->SetClass("popup-title", true);

	Rml::Element* content = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	content->SetAttribute("style", "display: flex; flex-direction: column; align-content: stretch; gap: 0.75em; width: 100%; height: 100%;");

	Rml::Element* textareaField = content->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	textareaField->SetAttribute("style", "display: flex; flex-direction: column; gap: 0.35em;");
	Rml::Element* textareaHint = textareaField->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	textareaHint->SetInnerRML("<div \"style\"=\"width: 1in; height: 1in; background-color: #cccccc;\"></div>Tell us what's working well, what's broken, or what would make the experience smoother.");
	textareaHint->SetAttribute("style", "font-size: 0.9em; color: #c0c0c0;");

	Rml::Element* textarea = textareaField->AppendChild(mainWindow.getRmlDocument()->CreateElement("textarea"));
	textarea->SetAttribute("style", "overflow-y: auto;");
	textarea->SetAttribute("rows", "5");
	textarea->SetAttribute("cols", "40");
	textarea->SetClass("pop-up-textarea", true);
	textarea->SetClass("surface-pop", true);

	Rml::Element* includeStateField = content->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	includeStateField->SetAttribute("style", "display: flex; align-items: flex-start; gap: 0.5em; padding: 0.6em; border-radius: 8dp; background-color: #262626;");
	Rml::Element* includeStateCheckbox = includeStateField->AppendChild(mainWindow.getRmlDocument()->CreateElement("input"));
	includeStateCheckbox->SetAttribute("type", "checkbox");
	includeStateCheckbox->SetAttribute("id", "feedback_include_state");
	includeStateCheckbox->SetAttribute("style", "margin-top: 0.2em;");

	includeStateField->AddEventListener(
		Rml::EventId::Click,
		new EventPasser(
			[includeStateCheckbox](Rml::Event& event){
				if (event.GetTargetElement() == includeStateCheckbox) return;
				if (includeStateCheckbox->HasAttribute("checked")) {
					includeStateCheckbox->RemoveAttribute("checked");
				} else {
					includeStateCheckbox->SetAttribute("checked", true);
				}
			}
		)
	);

	Rml::Element* includeStateCopy = includeStateField->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	includeStateCopy->SetAttribute("style", "display: flex; flex-direction: column; gap: 0.2em;");
	Rml::Element* includeStateLabel = includeStateCopy->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	includeStateLabel->SetInnerRML("Include app state");
	includeStateLabel->SetClass("popup-subtitle", true);
	Rml::Element* includeStateDescription = includeStateCopy->AppendChild(mainWindow.getRmlDocument()->CreateElement("p"));
	includeStateDescription->SetInnerRML("Shares a snapshot of your current logic graph and settings so we can reproduce the issue faster. No personal files are sent.");
	includeStateDescription->SetAttribute("style", "font-size: 0.85em; color: #c0c0c0;");

	Rml::Element* buttonContainer = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	buttonContainer->SetClass("popup-button-container", true);
	buttonContainer->SetAttribute("style", "display: flex; justify-content: flex-end; gap: 0.5em; width: 100%; margin-top: 0.75em;");

	Rml::Element* cancelButton = buttonContainer->AppendChild(mainWindow.getRmlDocument()->CreateElement("button"));
	cancelButton->SetInnerRML("Cancel");
	cancelButton->SetClass("popup-button", true);
	cancelButton->AddEventListener(Rml::EventId::Click, new EventPasser([popUpWindow](Rml::Event& event) {
		popUpWindow.destroy();
	}));

	Rml::Element* submitButton = buttonContainer->AppendChild(mainWindow.getRmlDocument()->CreateElement("button"));
	submitButton->SetInnerRML("Send Feedback");
	submitButton->SetClass("popup-button", true);
	submitButton->AddEventListener(
		Rml::EventId::Click,
		new EventPasser([this, popUpWindow, textarea, includeStateCheckbox](Rml::Event& event) {
			auto* textareaControl = dynamic_cast<Rml::ElementFormControlTextArea*>(textarea);
			std::string textareaValue = textareaControl ? textareaControl->GetValue().c_str() : "";

			// logInfo("Feedback body: {}", "", textareaValue);
			// logInfo("Include app state: {}", "", includeStateCheckbox->HasAttribute("checked") ? "true" : "false");
			std::vector<Network::Attachment> attachments;
			if (includeStateCheckbox->HasAttribute("checked")) {
				Network::Attachment appStateAttachment;
				std::string appState = App::get().dumpState().dump(1, '\t');
				std::optional<std::string> compressedAppState = compressString(appState);
				if (compressedAppState){
					appStateAttachment.data = compressedAppState.value();
					appStateAttachment.context = "app_state.json.json.br";
					appStateAttachment.contentType = "application/x-brotli";
				} else {
					appStateAttachment.data = appState;
					appStateAttachment.context = "app_state.json";
					appStateAttachment.contentType = "application/json";
				}
				attachments.push_back(appStateAttachment);
			}

			Network::get().sendFeedback(
				*this,
				"User Feedback from Connection Machine UI v" + getCurrentVersion().toString(),
				textareaValue,
				attachments
			);

			popUpWindow.destroy();
		})
	);
}


void PopUpManager::controlsConnectionMachine() {
	std::optional<PopUpWindow> optionalPopUpWindow = createPopUp(true);

	if (!optionalPopUpWindow) return;
	PopUpWindow popUpWindow = std::move(optionalPopUpWindow.value());

	// Rml::ElementList windowList;
    // overlay->GetElementsByClassName(windowList, "pop-up-window");
    // if (windowList.empty()) return;
    // Rml::Element* window = windowList.front();
	popUpWindow.getPopUpWindow()->SetAttribute("style", "display: flex; flex-direction: column; width: 520dp; height: 400dp; background-color:#303030;");

	Rml::Element* title = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	title->SetInnerRML("Controls");

	Rml::Element* leftandright = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	leftandright->SetAttribute("style", "display: flex; flex-direction: row; width: 100%; height: 100%; background-color:#303030;");



	std::vector<std::string> keybindSettingKeys = {
    	"Keybinds/File/Save",
    	"Keybinds/File/New",
    	"Keybinds/Editing/Undo",
    	"Keybinds/Editing/Redo",
    	"Keybinds/Editing/Copy",
    	"Keybinds/Editing/Paste",
    	"Keybinds/Editing/Rotate CCW",
    	"Keybinds/Editing/Rotate CW",
    	"Keybinds/Editing/Confirm",
    	"Keybinds/Camera/Zoom",
    	"Keybinds/Camera/Home"
	};
	std::vector<std::string> keybindSettingKeysNames = {
    	"Save",
    	"New",
    	"Undo",
    	"Redo",
    	"Copy",
    	"Paste",
    	"Rotate CCW",
    	"Rotate CW",
    	"Confirm",
    	"Zoom",
    	"Home"
	};
	std::vector<std::string> keybindSettingKeysNamesFiltered = {};

	SettingsMap& settings = Settings::getSettingsMap();
    std::vector<std::pair<std::string, std::string>> result;

	for (const auto& pair : settings.getAllKeys()) {

	    const std::string& key = pair.first;

		for (int i = 0; i < keybindSettingKeys.size(); ++i) {

			std::string keykeep = keybindSettingKeys[i];

	        if (keykeep == key) {
	            std::string value;

	            switch (pair.second->getType()) {

	            case SettingType::STRING:
	                value = *settings.get<SettingType::STRING>(key);
	                break;

	            case SettingType::INT:
	                value = std::to_string(*settings.get<SettingType::INT>(key));
	                break;

	            case SettingType::UINT:
	                value = std::to_string(*settings.get<SettingType::UINT>(key));
	                break;

	            case SettingType::DECIMAL:
	                value = std::to_string(*settings.get<SettingType::DECIMAL>(key));
	                break;

	            case SettingType::BOOL:
	                value = *settings.get<SettingType::BOOL>(key) ? "true" : "false";
	                break;

	            case SettingType::KEYBIND:
	                value = settings.get<SettingType::KEYBIND>(key)->toString(true);
	                break;

	            case SettingType::FILE_PATH:
	                value = *settings.get<SettingType::FILE_PATH>(key);
	                break;

	            case SettingType::VOID:
	            default:
	                value = "<void>";
	                break;
	            }

	            // Only call quoted on STRING, not on raw values
				std::ostringstream oss;
				oss << std::quoted(value);
				std::string quotedValue = oss.str();
				logInfo("key {}, value {}", "",key,quotedValue);
	            result.emplace_back(key, quotedValue);
				keybindSettingKeysNamesFiltered.push_back(keybindSettingKeysNames[i]);

	        }
	    }
	}

	Rml::Element* leftdiv = leftandright->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
	leftdiv->SetAttribute("style", "display: flex; flex-direction: column; width: 50%; height: 85%; background-color:#303030;");

	Rml::Element* rightdiv = leftandright->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
    rightdiv->SetAttribute("style", "display: flex; flex-direction: column; width: 50%; height: 85%; background-color:#303030;");
	int i = 0;
	for (const auto& pair : result) {
    	const std::string& key = pair.first;
    	const std::string& value = pair.second;

		Rml::Element* temp = leftdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		temp->SetInnerRML(keybindSettingKeysNames[i]);

		Rml::Element* temp2 = rightdiv->AppendChild(mainWindow.getRmlDocument()->CreateElement("div"));
		temp2->SetInnerRML(value.substr(1, value.length() - 2));
    	logInfo("key: {}, value: {}", "", key, value);
		i += 1;
	}

/* Settings to include
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save", Keybind(Keybind::KeyId::KI_S, Keybind::KeyMod::KM_CTRL));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/New", Keybind(Keybind::KeyId::KI_N, Keybind::KeyMod::KM_CTRL));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Undo", Keybind(Keybind::KeyId::KI_Z, Keybind::KeyMod::KM_CTRL));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Redo", Keybind(Keybind::KeyId::KI_Z, Keybind::KeyMod::KM_CTRL | Keybind::KeyMod::KM_SHIFT));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Copy", Keybind(Keybind::KeyId::KI_C, Keybind::KeyMod::KM_CTRL));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Paste", Keybind(Keybind::KeyId::KI_V, Keybind::KeyMod::KM_CTRL));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CCW", Keybind(Keybind::KeyId::KI_Q));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CW", Keybind(Keybind::KeyId::KI_E));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Confirm", Keybind(Keybind::KeyId::KI_E));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Zoom", Keybind(Keybind::KeyId::KI_UNKNOWN, Keybind::KeyMod::KM_SHIFT));
Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Home", Keybind(Keybind::KeyId::KI_F));
*/

	Rml::Element* close = popUpWindow.getPopUpWindow()->AppendChild(mainWindow.getRmlDocument()->CreateElement("button"));
    close->SetInnerRML("Close");
    close->SetClass("popup-button", true);
	close->AddEventListener(Rml::EventId::Click, new EventPasser(
	    [popUpWindow](Rml::Event& event) {
	        popUpWindow.destroy();
	    }
	));

}
