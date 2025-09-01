#include "pasteTool.h"

#include "../../circuitView.h"
#include "backend/backend.h"
#include "environment/environment.h"

void PasteTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Rotate Block CW", std::bind(&PasteTool::rotateCW, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CCW", std::bind(&PasteTool::rotateCCW, this, std::placeholders::_1));
	registerFunction("Tool Primary Activate", std::bind(&PasteTool::place, this, std::placeholders::_1));
}

bool PasteTool::rotateCW(const Event* event) {
	transformAmount.nextOrientation();
	elementId = 0; // remake elements
	updateElements();
	return true;
}

bool PasteTool::rotateCCW(const Event* event) {
	transformAmount.lastOrientation();
	elementId = 0; // remake elements
	updateElements();
	return true;
}

bool PasteTool::place(const Event* event) {
	SharedCopiedBlocks copiedBlocks = circuitView->getBackend()->getClipboard();
	if (copiedBlocks) circuit->tryInsertCopiedBlocks(copiedBlocks, lastPointerPosition, transformAmount);

	return true;
}

// Preview is only shown for the primary parsed circuit, not the dependencies that will be created in a different circuit
void PasteTool::updateElements() {
	if (!elementCreator.isSetup()) return;

	if (!pointerInView) {
		elementCreator.clear();
		return;
	}

	if (circuitView->getBackend()->getClipboardEditCounter() != lastClipboardEditCounter || !elementCreator.hasElement(elementId)) {
		lastClipboardEditCounter = circuitView->getBackend()->getClipboardEditCounter();
		// reset and remake blocks
		elementCreator.clear();

		SharedCopiedBlocks copiedBlocks = circuitView->getBackend()->getClipboard();
		if (!copiedBlocks) return;

		std::vector<BlockPreview::Block> blocks;
		blocks.reserve(copiedBlocks->getCopiedBlocks().size());
		for (const CopiedBlocks::CopiedBlockData& block : copiedBlocks->getCopiedBlocks()) {
			blocks.emplace_back(
				environment->getBlockRenderDataFeeder().getBlockRenderDataId(block.blockType),
				lastPointerPosition + transformAmount * (block.position - copiedBlocks->getMinPosition()) - transformAmount.transformVectorWithArea(Vector(0), circuit->getBlockContainer()->getBlockDataManager()->getBlockSize(block.blockType, block.orientation)),
				transformAmount * block.orientation
			);
		}
		elementId = elementCreator.addBlockPreview(BlockPreview(std::move(blocks)));
		lastElementPosition = lastPointerPosition;
	} else {
		// update old element
		elementCreator.shiftBlockPreview(elementId, lastPointerPosition - lastElementPosition);
		lastElementPosition = lastPointerPosition;
	}

}

bool PasteTool::validatePlacement() const {
	SharedCopiedBlocks copiedBlocks = circuitView->getBackend()->getClipboard();
	if (!copiedBlocks) return false;

	Vector totalOffset = Vector(lastPointerPosition.x, lastPointerPosition.y) + (Position() - copiedBlocks->getMinPosition());

	for (const CopiedBlocks::CopiedBlockData& block : copiedBlocks->getCopiedBlocks()) {
		Position testPos = block.position + totalOffset;
		if (circuit->getBlockContainer()->checkCollision(testPos)) {
			return false;
		}
	}
	return true;
}
