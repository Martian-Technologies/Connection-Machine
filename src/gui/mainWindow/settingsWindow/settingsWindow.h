#ifndef settingsWindow_h
#define settingsWindow_h

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "contentManager.h"

class SearchBar;

class SettingsWindow {
public:
	SettingsWindow(Rml::ElementDocument* document);
	~SettingsWindow();

	void toggleVisibility();
	void reloadContent();
	bool isVisible() const { return visible; };

private:
	void connectCategoryListeners(); // creates listeners for changing content-panels information
	// void connectWindowOptions(); // creates listeners for saving, reseting, or canceling the settings info

	Rml::Element* context;
	Rml::Element* activeNav;

	ContentManager contentManager;
	std::unique_ptr<SearchBar> searchBar;

	bool visible;
};

#endif /* settingsWindow_h */
