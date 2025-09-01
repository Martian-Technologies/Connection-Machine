#include "toolManagerManager.h"

std::map<std::string, std::unique_ptr<ToolManagerManager::BaseToolTypeMaker>> ToolManagerManager::tools;

#include "gui/viewportManager/circuitView/tools/connection/connectionTool.h"
#include "gui/viewportManager/circuitView/tools/movement/moveTool.h"
#include "gui/viewportManager/circuitView/tools/placement/blockPlacementTool.h"
#include "gui/viewportManager/circuitView/tools/selection/selectionMakerTool.h"
#include "gui/viewportManager/circuitView/tools/other/pasteTool.h"
#include "gui/viewportManager/circuitView/tools/other/logicToucher.h"
#include "gui/viewportManager/circuitView/tools/other/modeChangerTool.h"

ToolManagerManager::ToolManagerManager(DataUpdateEventManager* dataUpdateEventManager) : dataUpdateEventManager(dataUpdateEventManager) {
	ToolManagerManager::registerTool<ConnectionTool>();
	ToolManagerManager::registerTool<MoveTool>();
	ToolManagerManager::registerTool<BlockPlacementTool>();
	ToolManagerManager::registerTool<SelectionMakerTool>();
	ToolManagerManager::registerTool<PasteTool>();
	ToolManagerManager::registerTool<LogicToucher>();
	ToolManagerManager::registerTool<ModeChangerTool>();
	// ToolManagerManager::registerTool<PortSelector>(); // dont register the tool because it does not go in the menu
}
