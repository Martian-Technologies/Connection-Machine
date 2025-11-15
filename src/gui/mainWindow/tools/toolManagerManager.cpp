#include "toolManagerManager.h"

std::map<std::string, std::unique_ptr<ToolManagerManager::BaseToolTypeMaker>> ToolManagerManager::tools;

#include "gui/viewportManager/circuitView/tools/connection/connectionTool.h"
#include "gui/viewportManager/circuitView/tools/movement/moveTool.h"
#include "gui/viewportManager/circuitView/tools/other/logicToucher.h"
#include "gui/viewportManager/circuitView/tools/other/modeChangerTool.h"
#include "gui/viewportManager/circuitView/tools/other/pasteTool.h"
#include "gui/viewportManager/circuitView/tools/placement/blockPlacementTool.h"
#include "gui/viewportManager/circuitView/tools/selection/selectionMakerTool.h"

#include "environment/environment.h"

ToolManagerManager::ToolManagerManager(Environment& environment) : environment(environment), dataUpdateEventManager(environment.getBackend().getDataUpdateEventManager()) {
	ToolManagerManager::registerTool<ConnectionTool>();
	ToolManagerManager::registerTool<MoveTool>();
	ToolManagerManager::registerTool<BlockPlacementTool>();
	ToolManagerManager::registerTool<SelectionMakerTool>();
	ToolManagerManager::registerTool<PasteTool>();
	ToolManagerManager::registerTool<LogicToucher>();
	ToolManagerManager::registerTool<ModeChangerTool>();
	// ToolManagerManager::registerTool<PortSelector>(); // dont register the tool because it does not go in the menu
}

void ToolManagerManager::cycleActiveToolMode(int direction) {
	if (activeTool.empty()) {
		return;
	}

	auto toolIter = tools.find(activeTool);
	if (toolIter == tools.end()) {
		return;
	}

	const std::vector<std::string> modes = toolIter->second->getModes();
	if (modes.size() <= 1) {
		return;
	}

	int currentIndex = 0;
	auto modeIter = lastToolModes.find(activeTool);
	if (modeIter != lastToolModes.end()) {
		auto foundIter = std::find(modes.begin(), modes.end(), modeIter->second);
		if (foundIter != modes.end()) {
			currentIndex = std::distance(modes.begin(), foundIter);
		}
	}

	int newIndex = (currentIndex + direction) % modes.size();

	setMode(modes[newIndex]);
}
