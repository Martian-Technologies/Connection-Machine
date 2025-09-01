#include "logicToucher.h"

#include "../../circuitView.h"
#include "../../events/customEvents.h"

void LogicToucher::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&LogicToucher::press, this, std::placeholders::_1));
	registerFunction("tool primary deactivate", std::bind(&LogicToucher::unpress, this, std::placeholders::_1));
	registerFunction("Pointer Move", std::bind(&LogicToucher::pointerMove, this, std::placeholders::_1));
	setStatusBar("Left click to toggle the state of a block");
}


bool LogicToucher::press(const Event* event) {
	if (!circuitView || !circuit) return false;
	Evaluator* evaluator = circuitView->getEvaluator();
	if (!evaluator) return false;

	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (!positionEvent) return false;

	if (clicked && false) {
		return false;
	} else {
		clickPosition = lastPointerPosition;
		if (circuit->getBlockContainer()->checkCollision(clickPosition)) {
			bool stateToSet = true;
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, !evaluator->getBoolState(address));
		}
		clicked = true;
		return true;
	}
}

bool LogicToucher::unpress(const Event* event) {
	if (!circuitView || !circuit) return false;
	Evaluator* evaluator = circuitView->getEvaluator();
	if (!evaluator) return false;

	if (clicked) {
		const Block* block = circuit->getBlockContainer()->getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, false);
		}
		clicked = false;
		return true;
	}
	return false;
}

bool LogicToucher::pointerMove(const Event* event) {
	if (!circuitView || !circuit) return false;
	Evaluator* evaluator = circuitView->getEvaluator();
	if (!evaluator) return false;

	if (clicked && lastPointerPosition != clickPosition) {
		const Block* block = circuit->getBlockContainer()->getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, false);
		}
		clickPosition = lastPointerPosition;
		if (circuit->getBlockContainer()->checkCollision(clickPosition)) {
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, !evaluator->getBoolState(address));
		}
	}
	return false;
}
