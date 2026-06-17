#include "logicToucher.h"

#include "../../circuitView.h"
#include "../../events/customEvents.h"

void LogicToucher::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&LogicToucher::press, this, std::placeholders::_1));
	registerFunction("Tool Primary Deactivate", std::bind(&LogicToucher::unpress, this, std::placeholders::_1));
	registerFunction("Pointer Move", std::bind(&LogicToucher::pointerMove, this, std::placeholders::_1));
	setStatusBar("Left click to toggle the state of a block");
}

bool LogicToucher::press(const Event* event) {
	if (!circuitView || !getCircuit()) return false;
	EvalLogicSimulator* simulator = circuitView->getSimulator();
	if (!simulator) return false;

	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (!positionEvent) return false;

	if (clicked && false) {
		return false;
	} else {
		clickPosition = lastPointerPosition;
		const Block* block = getCircuit()->getBlockContainer().getBlock(clickPosition);
		if (block) {
			switch (block->type()) {
			case BlockType::CONSTANT_OFF: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_ON);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::HIGH));
			} break;
			case BlockType::CONSTANT_ON: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_Z: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_X: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			default:
				Address address = circuitView->getAddress();
				address.appendPosition(clickPosition);
				logic_state_t state = simulator->getState(address);
				state = (state == logic_state_t::HIGH) ? logic_state_t::LOW : logic_state_t::HIGH;
				simulator->setState(address, state);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, state));
			}
		}
		clicked = true;
		return true;
	}
}

bool LogicToucher::unpress(const Event* event) {
	if (!circuitView || !getCircuit()) return false;
	EvalLogicSimulator* simulator = circuitView->getSimulator();
	if (!simulator) return false;

	if (clicked) {
		const Block* block = getCircuit()->getBlockContainer().getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.appendPosition(clickPosition);
			simulator->setState(address, logic_state_t::LOW);
			sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
		}
		clicked = false;
		return true;
	}
	return false;
}

bool LogicToucher::pointerMove(const Event* event) {
	if (!circuitView || !getCircuit()) return false;
	EvalLogicSimulator* simulator = circuitView->getSimulator();
	if (!simulator) return false;

	if (clicked && lastPointerPosition != clickPosition) {
		const Block* block = getCircuit()->getBlockContainer().getBlock(clickPosition);
		if (block && block->type() == BlockType::BUTTON) {
			Address address = circuitView->getAddress();
			address.appendPosition(clickPosition);
			simulator->setState(address, logic_state_t::LOW);
		}
		clickPosition = lastPointerPosition;
		block = getCircuit()->getBlockContainer().getBlock(clickPosition);
		if (block) {
			switch (block->type()) {
			case BlockType::CONSTANT_OFF: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_ON);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::HIGH));
			} break;
			case BlockType::CONSTANT_ON: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_Z: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			case BlockType::CONSTANT_X: {
				getCircuit()->setType(block->getPosition(), BlockType::CONSTANT_OFF);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, logic_state_t::LOW));
			} break;
			default:
				Address address = circuitView->getAddress();
				address.appendPosition(clickPosition);
				logic_state_t state = simulator->getState(address);
				state = (state == logic_state_t::HIGH) ? logic_state_t::LOW : logic_state_t::HIGH;
				simulator->setState(address, state);
				sendEventToCircuitView(StateSetEvent("CircuitStateSet", clickPosition, state));
			}
		}
	}
	return false;
}
