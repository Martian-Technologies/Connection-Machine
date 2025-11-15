#include "blockPlacementTool.h"

#include "singlePlaceTool.h"
#include "areaPlaceTool.h"

BlockPlacementTool::BlockPlacementTool(const Environment& environment) : CircuitTool(environment) {
	activePlacementTool = std::make_shared<SinglePlaceTool>(environment);
}

void BlockPlacementTool::activate() {
	CircuitTool::activate();
	if (activePlacementTool) {
		toolStackInterface->pushTool(activePlacementTool);
		activePlacementTool->selectBlock(selectedBlock);
		// activePlacementTool->setRotation(rotation);
	}
}

void BlockPlacementTool::setMode(const std::string& mode) {
	if (this->mode != mode) {
		SharedBaseBlockPlacementTool newActivePlacementTool;
		if (mode == "Single") {
			newActivePlacementTool = std::make_shared<SinglePlaceTool>(environment);
			this->mode = mode;
		} else if (mode == "Area") {
			newActivePlacementTool = std::make_shared<AreaPlaceTool>(environment);
			this->mode = mode;
		} else {
			logError("Tool mode \"{}\" could not be found", "", mode);
			return;
		}
		activePlacementTool = newActivePlacementTool;
		toolStackInterface->popAbove(this);
	}
}
