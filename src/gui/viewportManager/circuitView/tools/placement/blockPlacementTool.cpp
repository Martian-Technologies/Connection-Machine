#include "blockPlacementTool.h"

#include "singlePlaceTool.h"
#include "areaPlaceTool.h"

BlockPlacementTool::BlockPlacementTool() {
	activePlacementTool = std::make_shared<SinglePlaceTool>();
}

void BlockPlacementTool::activate() {
	CircuitTool::activate();
	if (activePlacementTool) {
		toolStackInterface->pushTool(activePlacementTool);
		activePlacementTool->selectBlock(selectedBlock);
		// activePlacementTool->setRotation(rotation);
	}
}

void BlockPlacementTool::setMode(std::string toolMode) {
	if (mode != toolMode) {
		SharedBaseBlockPlacementTool newActivePlacementTool;
		if (toolMode == "Single") {
			newActivePlacementTool = std::make_shared<SinglePlaceTool>();
		} else if (toolMode == "Area") {
			newActivePlacementTool = std::make_shared<AreaPlaceTool>();
		} else {
			logError("Tool mode \"{}\" could not be found", "", toolMode);
			return;
		}
		activePlacementTool = newActivePlacementTool;
		toolStackInterface->popAbove(this);
	}
}
