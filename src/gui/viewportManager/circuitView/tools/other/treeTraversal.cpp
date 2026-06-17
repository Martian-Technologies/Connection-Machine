#include "treeTraversal.h"
#include "../../events/customEvents.h"
#include "environment/environment.h"
#include "gui/viewportManager/circuitView/circuitView.h"

void TreeTraversal::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&TreeTraversal::primary, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&TreeTraversal::secondary, this, std::placeholders::_1));
	if (circuitView == nullptr || circuitView->getAddress().size() == 0) {
		setStatusBar("Left click on a block to view it's internals.");
	} else {
		setStatusBar("Left click on a block to view it's internals. Right click to go to the parent circuit");
	}
}

bool TreeTraversal::primary(const Event* event) {
	if (!getCircuit() || !circuitView) return false;
	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (!positionEvent) return false;
	const Block* block = getCircuit()->getBlockContainer().getBlock(positionEvent->getPosition());
	if (!block) return false;
	const CircuitManager& circuitManager = environment.getBackend().getCircuitManager();
	circuit_id_t circuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(block->type());
	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuitId);
	if (!circuitBlockData) return false;
	Address address(circuitView->getAddress());
	address.appendPosition(block->getPosition());
	circuitView->setSimulator(circuitView->getSimulator(), address);
	return true;
}

bool TreeTraversal::secondary(const Event* event) {
	if (!getCircuit() || !circuitView) return false;
	if (circuitView->getAddress().size() == 0) return false;
	circuitView->setSimulator(circuitView->getSimulator()->getSimulatorId(), circuitView->getAddress().popBottomPosition());
	return true;
}

void TreeTraversal::updateElements() {
	if (!isActivate || !elementCreator.isSetup()) return;

	if (!pointerInView) {
		elementCreator.clear();
		return;
	}

	elementCreator.clear();

	const Block* block = getCircuit()->getBlockContainer().getBlock(lastPointerPosition);
	if (!block) {
		if (circuitView->getAddress().size() == 0) {
			setStatusBar("Left click on a block to view it's internals.");
		} else {
			setStatusBar("Left click on a block to view it's internals. Right click to go to the parent circuit");
		}
		return;
	}
	const CircuitManager& circuitManager = environment.getBackend().getCircuitManager();
	circuit_id_t circuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(block->type());
	const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuitId);
	if (!circuitBlockData) {
		elementCreator.addSelectionElement(SelectionElement(block->getPosition(), block->getLargestPosition(), true));
		setStatusBar("This block is not a circuit.");
		return;
	}
	elementCreator.addSelectionElement(SelectionElement(block->getPosition(), block->getLargestPosition(), false));
	setStatusBar("Click to enter the circuit.");
}
