#ifndef blockCreationWindow_h
#define blockCreationWindow_h

#include "gui/mainWindow/tools/toolManagerManager.h"
#include <RmlUi/Core.h>

class CircuitManager;
class MainWindow;

class BlockCreationWindow {
public:
	BlockCreationWindow(
		CircuitManager& circuitManager,
		Environment& environment,
		MainWindow& mainWindow,
		DataUpdateEventManager& dataUpdateEventManager,
		ToolManagerManager& toolManagerManager,
		Rml::ElementDocument* document,
		Rml::Element* menu
	);

	void updateFromMenu();
	void resetMenu();

private:
	void addListItem(
		bool isInput,
		bool findUnusedEndId = true,
		unsigned int endId = 0,
		const std::string& nameValue = "",
		Vector posOnBlockValue = Vector(),
		char portOffsetValue = 'C',
		std::optional<Position> posOfBlockValue = std::nullopt,
		unsigned int bitWidthValue = 1
	);
	void updateSelected(std::string string);
	void makePaths(std::vector<std::vector<std::string>>& paths, std::vector<std::string>& path, const EvalAddressTree& addressTree);

	Rml::ElementDocument* document;
	Rml::Element* outputList;
	Rml::Element* inputList;
	Rml::Element* menu;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	MainWindow& mainWindow;
	Environment& environment;
	CircuitManager& circuitManager;
	ToolManagerManager& toolManagerManager;
};

#endif /* blockCreationWindow_h */