#include "moveTool.h"

#include "../selectionHelpers/tensorCreationTool.h"
#include "../selectionHelpers/areaCreationTool.h"
#include "environment/environment.h"

void MoveTool::reset() {
	CircuitTool::reset();
	if (!activeSelectionHelper) {
		mode = "Area";
		activeSelectionHelper = std::make_shared<AreaCreationTool>();
	}
	transformAmount = Orientation();
	activeSelectionHelper->restart();
	updateElements();
}

void MoveTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Rotate Block CW", std::bind(&MoveTool::rotateCW, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CCW", std::bind(&MoveTool::rotateCCW, this, std::placeholders::_1));
	registerFunction("Tool Primary Activate", std::bind(&MoveTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&MoveTool::unclick, this, std::placeholders::_1));
	if (!activeSelectionHelper->isFinished()) {
		toolStackInterface->pushTool(activeSelectionHelper);
	} else {
		updateElements();
	}
}

void MoveTool::setMode(std::string toolMode) {
	if (mode != toolMode) {
		if (toolMode == "Area") {
			activeSelectionHelper = std::make_shared<AreaCreationTool>();
			mode = toolMode;
		} else if (toolMode == "Tensor") {
			activeSelectionHelper = std::make_shared<TensorCreationTool>();
			mode = toolMode;
		} else {
			logError("Tool mode \"{}\" could not be found", "", toolMode);
		}
		toolStackInterface->popAbove(this);
	}
}

bool MoveTool::rotateCW(const Event* event) {
	transformAmount.nextOrientation();
	elementId = 0; // remake elements
	updateElements();
	return true;
}

bool MoveTool::rotateCCW(const Event* event) {
	transformAmount.lastOrientation();
	elementId = 0; // remake elements
	updateElements();
	return true;
}

bool MoveTool::click(const Event* event) {
	if (!activeSelectionHelper->isFinished() || !circuit) return false;
	if (circuit->tryMoveBlocks(
		activeSelectionHelper->getSelection(),
		lastPointerPosition - getSelectionOrigin(activeSelectionHelper->getSelection()),
		transformAmount
	)) {
		reset();
		toolStackInterface->pushTool(activeSelectionHelper);
	}
	return true;
}

bool MoveTool::unclick(const Event* event) {
	if (!activeSelectionHelper->isFinished()) return false;
	elementCreator.clear();
	toolStackInterface->pushTool(activeSelectionHelper, false);
	return true;
}

void MoveTool::updateElements() {
	if (!elementCreator.isSetup()) return;

	if (!activeSelectionHelper->isFinished()) {
		elementCreator.clear();
		return;
	}
	setStatusBar("Left click to move the selected blocks.");
	if (!pointerInView) {
		elementCreator.clear();
		elementCreator.addSelectionElement(SelectionObjectElement(activeSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::SELECTION));
		return;
	}
	if (lastCircuitEdit != circuit->getEditCount() || !elementCreator.hasElement(elementId)) {
		lastCircuitEdit = circuit->getEditCount();
		elementCreator.clear();
		elementCreator.addSelectionElement(SelectionObjectElement(activeSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::SELECTION));
		Position selectionOrigin = getSelectionOrigin(activeSelectionHelper->getSelection());

		std::unordered_set<Position> positions;
		std::unordered_set<const Block*> blocksSet;
		bool foundPos = false;
		flattenSelection(activeSelectionHelper->getSelection(), positions);
		std::vector<BlockPreview::Block> blocks;
		for (Position position : positions) {
			const Block* block = circuit->getBlockContainer()->getBlock(position);
			if (!block) continue;
			if (blocksSet.contains(block)) continue;
			blocksSet.insert(block);
			blocks.emplace_back(
				environment->getBlockRenderDataFeeder().getBlockRenderDataId(block->type()),
				lastPointerPosition + transformAmount * (block->getPosition() - selectionOrigin) - transformAmount.transformVectorWithArea(Vector(0), block->size()),
				transformAmount * block->getOrientation()
			);
		}
		elementId = elementCreator.addBlockPreview(BlockPreview(std::move(blocks)));
		lastElementPosition = lastPointerPosition;
	} else {
		elementCreator.shiftBlockPreview(elementId, lastPointerPosition - lastElementPosition);
		lastElementPosition = lastPointerPosition;
	}
}
