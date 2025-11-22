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
		const Block* block = circuit->getBlockContainer().getBlock(clickPosition);
		if (block) {
			switch (block->type()) {
			case BlockType::CONSTANT_OFF: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_ON);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::HIGH));
			} break;
			case BlockType::CONSTANT_ON: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_Z: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_X: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			default:
				Address address = circuitView->getAddress();
				address.addBlockId(clickPosition);
				logic_state_t state = evaluator->getState(address);
				state = (state == logic_state_t::HIGH) ? logic_state_t::LOW : logic_state_t::HIGH;
				evaluator->setState(address, state);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, state));
			}
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
		const Block* block = circuit->getBlockContainer().getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, logic_state_t::LOW);
			sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
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
		const Block* block = circuit->getBlockContainer().getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.addBlockId(clickPosition);
			evaluator->setState(address, logic_state_t::LOW);
		}
		clickPosition = lastPointerPosition;
		block = circuit->getBlockContainer().getBlock(clickPosition);
		if (block) {
			switch (block->type()) {
			case BlockType::CONSTANT_OFF: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_ON);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::HIGH));
			} break;
			case BlockType::CONSTANT_ON: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_Z: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_X: {
				circuit->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			default:
				Address address = circuitView->getAddress();
				address.addBlockId(clickPosition);
				logic_state_t state = evaluator->getState(address);
				state = (state == logic_state_t::HIGH) ? logic_state_t::LOW : logic_state_t::HIGH;
				evaluator->setState(address, state);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, state));
			}
		}
	}
	return false;
}
