#include "singleConnectTool.h"

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
		if (!circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(lastPointerPosition).has_value()) {
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

	if (elementCreator.isSetup()) {
		elementCreator.clear();

		if (!(circuit && pointerInView)) return;

		if (clicked) {
			const Block* outputBlock = circuit->getBlockContainer().getBlock(clickPosition);
			if (!outputBlock) {
				reset();
				bool valid = circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(lastPointerPosition).has_value();
				elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
			} else {
				std::optional<ConnectionEnd> outputConnectionEnd = circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(clickPosition);
				if (!outputConnectionEnd) {
					reset();
					bool valid = circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(lastPointerPosition).has_value();
					elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
				} else {
					const Block* inputBlock = circuit->getBlockContainer().getBlock(lastPointerPosition);
					bool valid = false;
					if (inputBlock) {
						std::optional<connection_end_id_t> inputEndId = inputBlock->getInputOrBidirectionalConnectionId(lastPointerPosition);
						if (inputEndId) {
							elementCreator.addConnectionPreview(ConnectionPreview(
								clickPosition.free() + circuit->getBlockContainer().getBlockDataManager().getBlockData(outputBlock->type())->getConnectionPortOffset(
									outputConnectionEnd.value().getConnectionId(), outputBlock->getOrientation()).value(),
								lastPointerPosition.free() + circuit->getBlockContainer().getBlockDataManager().getBlockData(inputBlock->type())->getConnectionPortOffset(
									inputEndId.value(), inputBlock->getOrientation()).value()
							));
							valid = true;
						}
					}
					if (!valid) {
						elementCreator.addHalfConnectionPreview(HalfConnectionPreview(
							clickPosition.free() + circuit->getBlockContainer().getBlockDataManager().getBlockData(outputBlock->type())->getConnectionPortOffset(
									outputConnectionEnd.value().getConnectionId(), outputBlock->getOrientation()).value(),
							lastPointerFPosition
						));
					}
					elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
				}
			}
		} else {
			// TODO - change to use isvalid function
			bool valid = circuit->getBlockContainer().getOutputOrBidirectionalConnectionEnd(lastPointerPosition).has_value();
			elementCreator.addSelectionElement(SelectionElement(lastPointerPosition, !valid));
		}
	}

	if (clicked) {
		setStatusBar("Left click to set the connection end. Remake a connection to remove it.");
	} else {
		setStatusBar("Left click to set the connection start. Remake a connection to remove it.");
	}
}
