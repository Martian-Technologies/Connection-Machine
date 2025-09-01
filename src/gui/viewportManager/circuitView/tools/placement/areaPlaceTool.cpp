#include "areaPlaceTool.h"
#include "gpu/mainRenderer.h"

void AreaPlaceTool::activate() {
	BaseBlockPlacementTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&AreaPlaceTool::startPlaceBlock, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&AreaPlaceTool::startDeleteBlocks, this, std::placeholders::_1));
}

bool AreaPlaceTool::startPlaceBlock(const Event* event) {
	if (!circuit) return false;
	switch (click) {
	case 'n':
		click = 'p';
		clickPosition = lastPointerPosition;
		updateElements();
		break;
	case 'p':
		circuit->tryInsertOverArea(clickPosition, lastPointerPosition, orientation, selectedBlock);
		click = 'n';
		updateElements();
		break;
	case 'r':
		circuit->tryRemoveOverArea(clickPosition, lastPointerPosition);
		click = 'n';
		updateElements();
		break;
	}
	return true;
}

bool AreaPlaceTool::startDeleteBlocks(const Event* event) {
	if (!circuit) return false;
	switch (click) {
	case 'n':
		click = 'r';
		clickPosition = lastPointerPosition;
		updateElements();
		return true;
	default:
		click = 'n';
		updateElements();
		return true;
	}
}

void AreaPlaceTool::updateElements() {
	if (!elementCreator.isSetup()) return;
	elementCreator.clear();
	switch (click) {
	case 'n':
		if (pointerInView) {
			elementCreator.addSelectionElement(SelectionElement(lastPointerPosition));
		}
		setStatusBar("Select first corner. Left click to place. Right click to remove.");
		break;
	case 'p':
		elementCreator.addSelectionElement(SelectionElement(clickPosition, lastPointerPosition));
		setStatusBar("Select second corner. Left click to place. Right click to cancel.");
		break;
	case 'r':
		elementCreator.addSelectionElement(SelectionElement(clickPosition, lastPointerPosition, true));
		setStatusBar("Select second corner. Left click to remove. Right click to cancel.");
		break;
	}
}
