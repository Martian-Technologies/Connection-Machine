#include "singleConnectTool.h"
#include "gui/viewportManager/circuitView/renderer/logicRenderingUtils.h"

bool SingleConnectTool::makeConnection(const Event* event) {
	if (!circuit) return false;

	if (clicked) {
		if (!circuit->tryRemoveConnection(clickPosition, lastPointerPosition)) {
			circuit->tryCreateConnection(clickPosition, lastPointerPosition);
		}
		reset();
		updateElements();
		return true;
	} else {
		if (!circuit->getBlockContainer()->getOutputConnectionEnd(lastPointerPosition).has_value()) {
			return false;
		}

		clicked = true;
		clickPosition = lastPointerPosition;
		updateElements();
		return true;
	}
}

bool SingleConnectTool::cancelConnection(const Event* event) {
	if (clicked) {
		reset();
		updateElements();
		return true;
	}
	return false;
}

void SingleConnectTool::updateElements() {
	if (!isActivate) return;
	if (clicked) {
		setStatusBar("Left click to set the connection end. Remake a connection to remove it.");
	} else {
		setStatusBar("Left click to set the connection start. Remake a connection to remove it.");
	}

	if (!elementCreator.isSetup()) return;
	elementCreator.clear();

	if (!(circuit && pointerInView)) return;

	if (clicked) {
		bool valid = circuit->getBlockContainer()->getInputConnectionEnd(lastPointerPosition).has_value();

		const Block* outputBlock = circuit->getBlockContainer()->getBlock(clickPosition);
		if (valid) {
			const Block* inputBlock = circuit->getBlockContainer()->getBlock(lastPointerPosition);
			elementCreator.addConnectionPreview(ConnectionPreview(
				clickPosition.free() + getOutputOffset(outputBlock->type(), outputBlock->getOrientation()),
				lastPointerPosition.free() + getInputOffset(inputBlock->type(), inputBlock->getOrientation())
			));
		} else {
			elementCreator.addHalfConnectionPreview(HalfConnectionPreview(
				clickPosition.free() + getOutputOffset(outputBlock->type(), outputBlock->getOrientation()),
				lastPointerFPosition
			));
		}

		elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
	} else {
		// TODO - change to use isvalid function
		bool valid = circuit->getBlockContainer()->getOutputConnectionEnd(lastPointerPosition).has_value();
		elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
	}
}
