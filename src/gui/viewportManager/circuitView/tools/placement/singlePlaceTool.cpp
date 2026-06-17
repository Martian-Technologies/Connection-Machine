#include "singlePlaceTool.h"
#include "environment/environment.h"

void SinglePlaceTool::activate() {
	BaseBlockPlacementTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&SinglePlaceTool::startPlaceBlock, this, std::placeholders::_1));
	registerFunction("Tool Primary Deactivate", std::bind(&SinglePlaceTool::stopPlaceBlock, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&SinglePlaceTool::startDeleteBlocks, this, std::placeholders::_1));
	registerFunction("Tool Secondary Deactivate", std::bind(&SinglePlaceTool::stopDeleteBlocks, this, std::placeholders::_1));
	registerFunction("Pointer Move", std::bind(&SinglePlaceTool::pointerMove, this, std::placeholders::_1));
	setStatusBar("Left Click to Place. Right Click to Remove.");
}

bool SinglePlaceTool::startPlaceBlock(const Event* event) {
	if (!getCircuit()) return false;

	switch (clicks[0]) {
	case 'n':
		clicks[0] = 'p';
		if (selectedBlock != BlockType::NONE) getCircuit()->tryInsertBlock(lastPointerPosition - calculateElementOffset(), orientation, selectedBlock);
		updateElements();
		return true;
	case 'p':
		return false;
	case 'r':
		if (clicks[1] == 'n') {
			clicks[1] = 'p';
			if (selectedBlock != BlockType::NONE) getCircuit()->tryInsertBlock(lastPointerPosition - calculateElementOffset(), orientation, selectedBlock);
			updateElements();
			return true;
		}
		return false;
	}
	return false;
}

bool SinglePlaceTool::stopPlaceBlock(const Event* event) {
	// this logic is to allow holding right then left then releasing left and it to start deleting
	switch (clicks[0]) {
	case 'n':
		return false;
	case 'p':
		if (clicks[1] == 'r') {
			clicks[0] = 'r';
			clicks[1] = 'n';
		} else {
			clicks[0] = 'n';
		}
		return true;
	case 'r':
		if (clicks[1] == 'p') {
			clicks[1] = 'n';
			return true;
		}
		return false;
	}
	return false;
}

bool SinglePlaceTool::startDeleteBlocks(const Event* event) {
	if (!getCircuit()) return false;
	switch (clicks[0]) {
	case 'n':
		clicks[0] = 'r';
		getCircuit()->tryRemoveBlock(lastPointerPosition);
		updateElements();
		return true;
	case 'r':
		return false;
	case 'p':
		if (clicks[1] == 'n') {
			clicks[1] = 'r';
			getCircuit()->tryRemoveBlock(lastPointerPosition);
			updateElements();
			return true;
		}
		return false;
	}
	return false;
}

bool SinglePlaceTool::stopDeleteBlocks(const Event* event) {
	// this logic is to allow holding left then right then releasing right and it to start placing
	switch (clicks[0]) {
	case 'n':
		return false;
	case 'r':
		if (clicks[1] == 'p') {
			clicks[0] = 'p';
			clicks[1] = 'n';
		} else {
			clicks[0] = 'n';
		}
		return true;
	case 'p':
		if (clicks[1] == 'r') {
			clicks[1] = 'n';
			return true;
		}
		return false;
	}
	return false;
}

bool SinglePlaceTool::pointerMove(const Event* event) {
	if (!getCircuit()) return false;

	bool returnVal = false; // used to make sure it updates the effect
	updateElements();

	switch (clicks[0]) {
	case 'n':
		return false;
	case 'r':
		if (clicks[1] == 'p') {
			if (selectedBlock != BlockType::NONE) getCircuit()->tryInsertBlock(lastPointerPosition - calculateElementOffset(), orientation, selectedBlock);
		} else {
			getCircuit()->tryRemoveBlock(lastPointerPosition);
		}
		return false;
	case 'p':
		if (clicks[1] == 'r') getCircuit()->tryRemoveBlock(lastPointerPosition);
		else if (selectedBlock != BlockType::NONE) getCircuit()->tryInsertBlock(lastPointerPosition - calculateElementOffset(), orientation, selectedBlock);
	}
	return false;
}

void SinglePlaceTool::updateElements() {
	if (!isActivate || !elementCreator.isSetup()) return;
	elementCreator.clear();

	if (!pointerInView) return;
	if (selectedBlock != BlockType::NONE) {
		if (!getCircuit()) return;
		Size size = getCircuit()->getBlockContainer().getBlockDataManager().getBlockSize(selectedBlock, orientation);
		Position placingPosition = lastPointerPosition - calculateElementOffset();
		bool cantPlace = getCircuit()->getBlockContainer().checkCollision(placingPosition, orientation, selectedBlock);
		elementCreator.addSelectionElement(SelectionElement(placingPosition, placingPosition + size.getLargestVectorInArea(), cantPlace));
		elementCreator.addBlockPreview(BlockPreview(environment.getBlockRenderDataFeeder().getBlockRenderDataId(selectedBlock), placingPosition, orientation));
	} else {
		elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, true));
	}
}

Vector SinglePlaceTool::calculateElementOffset() const {
	if (!getCircuit()) return Vector(0, 0);
	if (selectedBlock == BlockType::NONE) return Vector(0, 0);
	if (selectedBlock == BlockType::JUNCTION_L || selectedBlock == BlockType::JUNCTION_H) {
		return orientation.transformVectorWithArea(Vector(0, 2), getCircuit()->getBlockContainer().getBlockDataManager().getBlockSize(selectedBlock));
	}
	return Vector(0, 0);
}
