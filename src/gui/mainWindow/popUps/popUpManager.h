#ifndef popUpManager_h
#define popUpManager_h

#include "RmlUi/Core/Element.h"

class MainWindow;

class PopUpManager {
public:
	class PopUpWindow {
	public:
		PopUpWindow(Rml::Context* context, Rml::Element* element, std::function<void()> destroyFunc) : context(context), element(element), destroyFunc(destroyFunc) { }

		Rml::Context* getContext() { return context; }
		Rml::Context* getContext() const { return context; }
		// Rml::Element* getElement() { return element; }
		// Rml::Element* getElement() const { return element; }
		Rml::Element* getPopUpWindow() {
			Rml::ElementList windowList;
			element->GetElementsByClassName(windowList, "pop-up-window");
			if (windowList.empty()) return nullptr;
			return windowList.front();
		}
		Rml::Element* getPopUpWindow() const {
			Rml::ElementList windowList;
			element->GetElementsByClassName(windowList, "pop-up-window");
			if (windowList.empty()) return nullptr;
			return windowList.front();
		}
		void destroy() const { destroyFunc(); }

	private:
		Rml::Context* context;
		Rml::Element* element;
		std::function<void()> destroyFunc;
	};

	PopUpManager(MainWindow& mainWindow) : mainWindow(mainWindow) { }
	std::optional<PopUpManager::PopUpWindow> createPopUp(bool blocking = true, const std::string& windowName = "", unsigned int width = 400, unsigned int height = 300);

	void addOptionsPopUp(const std::string& message, const std::vector<std::pair<std::string, std::function<void()>>>& options, bool blocking = true) {
		addOptionsPopUp(message, std::nullopt, options, blocking);
	}
	void addOptionsPopUp(
		const std::string& message,
		const std::optional<std::string>& smallMessage,
		const std::vector<std::pair<std::string, std::function<void()>>>& options,
		bool blocking = true
	);

	void savePopUp(const std::string& circuitUUID);
	void saveAsPopUp(const std::string& circuitUUID);
	void addFeedbackPopup();
	void aboutConnectionMachine();
	void controlsConnectionMachine();

private:
	MainWindow& mainWindow;
};

#endif /* popUpManager_h */