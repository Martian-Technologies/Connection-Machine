#include "settingsWindow.h"

#include "gui/helper/eventPasser.h"
#include "searchBar.h"

#include <RmlUi/Core/Element.h>

SettingsWindow::SettingsWindow(Rml::ElementDocument* document) : contentManager(document), visible(false) {
	context = document->GetElementById("settings-overlay");

	context->GetElementById("settings-close")->AddEventListener(Rml::EventId::Click, new EventPasser([this](Rml::Event& event) { toggleVisibility(); }));

	searchBar = std::make_unique<SearchBar>(document);

	// connectCategoryListeners();
	// connectWindowOptions();

	reloadContent();
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::connectCategoryListeners() {
	// Rml::ElementList items;
	// Rml::ElementUtilities::GetElementsByClassName(items, context->GetElementById("navigation-panel"), "nav-item");

	// for (size_t i = 0; i < items.size(); i++) {
	// 	Rml::Element* element = items[i];
	// 	if (i == 0) {
	// 		activeNav = element;
	// 		element->SetClass("active-nav", true);
	// 	}

	// 	element->AddEventListener("click", new EventPasser(
	// 		[this](Rml::Event& event) {
	// 			Rml::Element* current = event.GetCurrentElement();

	// 			activeNav->SetClass("active-nav", false);
	// 			activeNav = current;
	// 			activeNav->SetClass("active-nav", true);

	// 			std::string loadFile = current->GetId().substr(4) + ".rml";
	// 			logInfo(loadFile);
	// 		}
	// 	));
	// }

}

// void SettingsWindow::connectWindowOptions() {
// 	Rml::Element* settingsActions = context->GetElementById("setting-actions");

// 	// Save
// 	Rml::Element* saveButton = settingsActions->GetElementById("settings-apply");
// 	saveButton->AddEventListener("click", new EventPasser(
// 		[this](Rml::Event& event) {
// 			logInfo("saved");
// 		}
// 	));

// 	// Default
// 	Rml::Element* defaultButton = settingsActions->GetElementById("settings-reset");
// 	defaultButton->AddEventListener("click", new EventPasser(
// 		[this](Rml::Event& event) {
// 			logInfo("default");
// 		}
// 	));

// 	// Cancel
// 	Rml::Element* closeButton = settingsActions->GetElementById("settings-cancel");
// 	closeButton->AddEventListener("click", new EventPasser(
// 		[this](Rml::Event& event) {
// 			logInfo("closed");
// 		}
// 	));
// }

void SettingsWindow::toggleVisibility() {
	visible = !visible;
	context->SetClass("invisible", !visible);
	if (visible) {
		reloadContent();
	}
}

void SettingsWindow::reloadContent() {
	contentManager.load();
	if (searchBar) {
		searchBar->reapplyFilter();
	}
}
