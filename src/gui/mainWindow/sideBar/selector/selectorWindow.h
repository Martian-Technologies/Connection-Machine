#ifndef selectorWindow_h
#define selectorWindow_h

#include "backend/blockData/blockDataManager.h"
#include "gui/helper/elementList.h"
#include "gui/helper/menuTree.h"
#include "gui/mainWindow/tools/toolManagerManager.h"

class SelectorWindow {
public:
	SelectorWindow(
		BlockDataManager& blockDataManager,
		DataUpdateEventManager& dataUpdateEventManager,
		ProceduralCircuitManager& proceduralCircuitManager,
		ToolManagerManager& toolManagerManager,
		Rml::ElementDocument* document
	);

	void updateToolModeOptions();
	void updateList();
	void refreshSidebar(bool rebuildItems = false);

private:
	void updateSelected(const std::string& string);
	void updateSelectedMode(const std::string& string);
	void setupParameterMenu();
	void setupProceduralCircuitParameterMenu();
	void setupBusParameterMenu();
	void addParametersToParameterMenu(const ProceduralCircuitParameters& parameters, const std::string& title);
	Rml::ElementPtr makeParameterElement(const std::string& name, int defaultValue);
	void hideParameterMenu();

	void highlightActiveToolInSidebar();
	void highlightActiveMode();

	bool selectedBus = false;
	SharedProceduralCircuit selectedProceduralCircuit = nullptr;

	Rml::ElementDocument* document;
	Rml::Element* parameterMenu;
	std::optional<MenuTree> menuTree;
	std::optional<ElementList> modeList;
	BlockDataManager& blockDataManager;
	ProceduralCircuitManager& proceduralCircuitManager;
	ToolManagerManager& toolManagerManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
};

#endif /* selectorWindow_h */