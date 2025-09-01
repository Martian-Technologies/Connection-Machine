#include "portSelector.h"
#include "../../events/customEvents.h"

void PortSelector::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&PortSelector::press, this, std::placeholders::_1));
	setStatusBar("Left click to set the location a port connects to.");
}

bool PortSelector::press(const Event* event) {
	if (type == BlockType::NONE || !circuit || circuit->getBlockType() != type || !circuitView) return false;
	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (!positionEvent) return false;
	onSelectFunction(positionEvent->getPosition());
	reset();
	toolStackInterface->switchToStack(0);
	return true;
}
