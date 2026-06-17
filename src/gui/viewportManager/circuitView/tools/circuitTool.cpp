#include "circuitTool.h"

#include "../events/eventRegister.h"
#include "../events/customEvents.h"
#include "gpu/mainRendererDefs.h"
#include "../circuitView.h"

void CircuitTool::sendEventToCircuitView(const Event& event) {
	eventRegister->doEvent(event);
}

bool CircuitTool::sendEvent(const Event* event) {
	auto events = registeredEvents;
	for (const auto& pair : events) {
		if (pair.first == event->getName()) {
			auto func = eventRegister->getEventFunction(pair.first, pair.second);
			if (func != std::nullopt) {
				if ((*func)(event)) return true;
			}
		}
	}
	return false;
}

Circuit* CircuitTool::getCircuit() {
	if (circuitView == nullptr) return nullptr;
	return circuitView->getCircuit();
}

const Circuit* CircuitTool::getCircuit() const {
	if (circuitView == nullptr) return nullptr;
	return circuitView->getCircuit();
}

void CircuitTool::registerFunction(const std::string& eventName, const EventFunction& function) {
registeredEvents.emplace_back(eventName, eventRegister->registerFunction(eventName, function));
}

void CircuitTool::unregisterFunction(const std::string& eventName) {
	for (auto iter = registeredEvents.begin(); iter != registeredEvents.end(); iter++) {
		if (iter->first == eventName) {
			eventRegister->unregisterFunction(iter->first, iter->second);
			*iter = registeredEvents.back();
			registeredEvents.pop_back();
		}
	}
}

void CircuitTool::unregisterFunctions() {
	for (const auto& eventFuncPair : registeredEvents) {
		eventRegister->unregisterFunction(eventFuncPair.first, eventFuncPair.second);
	}
	registeredEvents.clear();
}

void CircuitTool::setStatusBar(const std::string& text) {
	eventRegister->doEvent(EventWithValue<std::string>("status bar changed", text));
}

void CircuitTool::activate() {
	setStatusBar("");
	registerFunction("pointer enter view", std::bind(&CircuitTool::enterBlockView, this, std::placeholders::_1));
	registerFunction("pointer exit view", std::bind(&CircuitTool::exitBlockView, this, std::placeholders::_1));
	registerFunction("Pointer Move", std::bind(&CircuitTool::pointerMove, this, std::placeholders::_1));
	isActivate = true;
	updateElements();
}

void CircuitTool::setup(ViewportId viewportId, EventRegister& eventRegister, ToolStackInterface& toolStackInterface, CircuitView& circuitView) {
	this->toolStackInterface = &toolStackInterface;
	this->elementCreator.setup(viewportId);
	this->eventRegister = &eventRegister;
	this->circuitView = &circuitView;
}

void CircuitTool::unsetup() {
	deactivate(true);
}

bool CircuitTool::enterBlockView(const Event* event) {
	pointerInView = true;
	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (positionEvent) {
		lastPointerFPosition = positionEvent->getFPosition();
		lastPointerPosition = positionEvent->getPosition();
	}
	updateElements();
	return false;
}

bool CircuitTool::exitBlockView(const Event* event) {
	pointerInView = false;
	updateElements();
	return false;
}

bool CircuitTool::pointerMove(const Event* event) {
	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (positionEvent) {
		lastPointerFPosition = positionEvent->getFPosition();
		lastPointerPosition = positionEvent->getPosition();
	}
	updateElements();
	return false;
}
