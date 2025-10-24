#include "popUpManager.h"

#include "gui/helper/eventPasser.h"
#include "gui/mainWindow/mainWindow.h"
#include "environment/environment.h"
#include "gui/helper/saveCallback.h"
#include "computerAPI/directoryManager.h"
#include "app.h"

#include <RmlUi/Debugger.h>

#include <SDL3/SDL.h>

void PopUpManager::addOptionsPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options, bool blocking) {
	std::pair<Rml::Element*, std::function<void()>> popUpData = createPopUp(blocking);
	Rml::Element* popUpRoot = popUpData.first;
	Rml::ElementList windowList;
	popUpRoot->GetElementsByClassName(windowList, "pop-up-window");
	Rml::Element* window = windowList.front();
	Rml::Element* text = window->AppendChild(mainWindow->getRmlDocument()->CreateElement("span"));
	text->SetInnerRML(message);
	text->SetClass("pop-up-text", true);
	Rml::Element* actionsElement = window->AppendChild(mainWindow->getRmlDocument()->CreateElement("span"));
	actionsElement->SetClass("pop-up-actions", true);
	for (const auto& option : options) {
		Rml::ElementPtr setPositionButton = mainWindow->getRmlDocument()->CreateElement("button");
		setPositionButton->AppendChild(std::move(mainWindow->getRmlDocument()->CreateTextNode(option.first)));
		setPositionButton->AddEventListener(Rml::EventId::Click, new EventPasser(
			[deleteFunc = popUpData.second, func = option.second](Rml::Event& event) {
				deleteFunc();
				func();
			}
		));
		setPositionButton->SetClass("pop-up-action", true);
		actionsElement->AppendChild(std::move(setPositionButton));
	}
}

std::pair<Rml::Element*, std::function<void()>> PopUpManager::createPopUp(bool blocking, const std::string& windowName, unsigned int width, unsigned int height) {
	if (blocking) {
		Rml::Element* allPopUps = mainWindow->getRmlDocument()->GetElementById("all-pop-ups");
		allPopUps->SetClass("invisible", false);
		assert(allPopUps);
		Rml::Element* popUpOverlay = allPopUps->AppendChild(mainWindow->getRmlDocument()->CreateElement("div"));
		popUpOverlay->SetClass("pop-up-overlay", true);
		Rml::Element* popUpOverlayInternal = popUpOverlay->AppendChild(mainWindow->getRmlDocument()->CreateElement("div"));
		popUpOverlayInternal->SetClass("pop-up-overlay-internal", true);
		Rml::Element* popUpWindow = popUpOverlayInternal->AppendChild(mainWindow->getRmlDocument()->CreateElement("div"));
		popUpWindow->SetClass("pop-up-window", true);
		popUpWindow->SetClass("pop-up-window-blocked", true);
		popUpWindow->SetClass("bordered", true);
		popUpWindow->SetClass("bg-3", true);
		return {popUpOverlay, [popUpOverlay, allPopUps](){
			App::get().queForEndOfUpdate([popUpOverlay, allPopUps](){
				allPopUps->RemoveChild(popUpOverlay);
				if (!allPopUps->HasChildNodes()) allPopUps->SetClass("invisible", true);
			});
		}};
	} else {
		SdlWindow* sdlWindow = App::get().registerWindow(windowName, width, height).get();
		WindowId windowId = MainRenderer::get().registerWindow(sdlWindow);
		Rml::Context* rmlContext = Rml::CreateContext("popup_" + std::to_string(windowId), Rml::Vector2i(sdlWindow->getSize().first, sdlWindow->getSize().second));
		if (rmlContext) {
			Rml::ElementDocument* rmlDocument = rmlContext->LoadDocument(DirectoryManager::getResourceDirectory().generic_string() + "/gui/mainWindow/popUpWindow/popUpWindow.rml");
			sdlWindow->setRenderFunction([windowId, rmlContext](){
				RmlRenderInterface* rmlRenderInterface = dynamic_cast<RmlRenderInterface*>(Rml::GetRenderInterface());
				if (rmlRenderInterface) {
					rmlContext->Update();
					rmlRenderInterface->setWindowToRenderOn(windowId);
					MainRenderer::get().prepareForRmlRender(windowId);
					rmlContext->Render();
					MainRenderer::get().endRmlRender(windowId);
				}
			});
			sdlWindow->setRecieveEventFunction(
				[windowId, rmlContext, sdlWindow](SDL_Event& event){
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
				}
			);
			rmlDocument->Show();
			return {rmlDocument, [windowId, rmlContext, sdlWindow](){
				App::get().queForEndOfUpdate([windowId, rmlContext, sdlWindow](){
					Rml::RemoveContext(rmlContext->GetName());
					MainRenderer::get().deregisterWindow(windowId);
					App::get().deregisterWindow(sdlWindow);
				});
			}};
		}
		logError("Failed to create new window for popup :(");
		return {nullptr, nullptr};
	}
}

void PopUpManager::savePopUp(const std::string& circuitUUID) {
	if (!mainWindow->getEnvironment()->getCircuitFileManager().save(circuitUUID)) {
		// if failed to save the circuit with out a path
		saveAsPopUp(circuitUUID);
	}
}

void PopUpManager::saveAsPopUp(const std::string& circuitUUID) {
	static const SDL_DialogFileFilter filters[] = {
		{ "Circuit Files", "cir" }
	};
	std::pair<CircuitFileManager*, std::string>* data = new std::pair<CircuitFileManager*, std::string>(&mainWindow->getEnvironment()->getCircuitFileManager(), circuitUUID);
	SDL_ShowSaveFileDialog(SaveCallback, data, nullptr, filters, 1, nullptr);
}

void PopUpManager::addFeedbackPopup() {
	std::pair<Rml::Element*, std::function<void()>> popUpData = createPopUp(false);
	Rml::Element* popUpRoot = popUpData.first;
	Rml::ElementList windowList;
	popUpRoot->GetElementsByClassName(windowList, "pop-up-window");
	Rml::Element* window = windowList.front();

	Rml::Element* text = window->AppendChild(mainWindow->getRmlDocument()->CreateElement("span"));
	text->SetInnerRML("Testing");
	text->SetClass("pop-up-text", true);
}