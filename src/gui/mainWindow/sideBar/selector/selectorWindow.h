#ifndef selectorWindow_h
#define selectorWindow_h

#include "backend/blockData/blockDataManager.h"
#include "gui/mainWindow/tools/toolManagerManager.h"
#include "gui/helper/menuTree.h"
#include "gui/helper/elementList.h"

class SelectorWindow {
public:
	SelectorWindow(
		const BlockDataManager* blockDataManager,
		DataUpdateEventManager* dataUpdateEventManager,
		ProceduralCircuitManager* proceduralCircuitManager,
		ToolManagerManager* toolManagerManager,
		Rml::ElementDocument* document
	);

	void updateToolModeOptions();
	void updateList();
	void refreshSidebar(bool rebuildItems = false);

private:
	void updateSelected(const std::string& string);
	void updateSelectedMode(const std::string& string);
	void setupProceduralCircuitParameterMenu();
	void hideProceduralCircuitParameterMenu();

	void highlightActiveToolInSidebar();
	void highlightActiveMode();

	SharedProceduralCircuit selectedProceduralCircuit = nullptr;

	Rml::ElementDocument* document;
	Rml::Element* parameterMenu;
	std::optional<MenuTree> menuTree;
	std::optional<ElementList> modeList;
	const BlockDataManager* blockDataManager;
	ProceduralCircuitManager* proceduralCircuitManager;
	ToolManagerManager* toolManagerManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
};

#endif /* selectorWindow_h */