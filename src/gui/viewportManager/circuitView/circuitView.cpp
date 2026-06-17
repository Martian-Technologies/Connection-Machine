#include "circuitView.h"

#include "backend/backend.h"
#include "environment/environment.h"
#include "events/customEvents.h"

#include "gpu/mainRenderer.h"

CircuitView::CircuitView(Environment& environment, ViewportId viewportId) :
	environment(environment), backend(environment.getBackend()), dataUpdateEventManager(environment.getBackend().getDataUpdateEventManager()), viewportId(viewportId),
	toolManager(environment, eventRegister, viewportId, *this), tutorialManager(environment, *this), viewManager(environment.getBackend()) {
	MainRenderer::get().moveViewportView(viewportId, viewManager.getTopLeft(), viewManager.getBottomRight());
	viewManager.setUpEvents(eventRegister);
	viewManager.connectViewChanged(std::bind(&CircuitView::viewChanged, this));
}

Circuit* CircuitView::getCircuit() { return backend.getCircuitManager().getCircuit(circuitId); }
const Circuit* CircuitView::getCircuit() const { return backend.getCircuitManager().getCircuit(circuitId); }

EvalLogicSimulator* CircuitView::getSimulator() { return backend.getSimulator(simulatorId); }
const EvalLogicSimulator* CircuitView::getSimulator() const { return backend.getSimulator(simulatorId); }

void CircuitView::setSimulator(simulator_id_t simulatorId, const Address& address) {
	if (simulatorId == 0) {
		this->simulatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportSimulator(viewportId, nullptr, Address());
		toolManager.setCircuit(0);
		viewManager.setCircuit(0);
		dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else {
		const EvalLogicSimulator* simulator = backend.getSimulatorManager().getSimulator(simulatorId);
		if (simulator == nullptr) {
			logError("When setting CircuitView's simulator, a simulator with a different backend. Failed to connect! Doing nothing!", "CircuitView");
		} else {
			circuit_id_t circuitId = simulator->getCircuitId(address);
			Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId); // ok if null
			if (circuit == nullptr){
				logError("When setting CircuitView's simulator, failed to find circuit for simulator with circuit id {}", "CircuitView", circuitId);
				return;
			}
			this->simulatorId = simulatorId;
			this->address = address;
			this->circuitId = circuitId;
			circuitRenderManager.reset();
			MainRenderer::get().setViewportSimulator(viewportId, simulator, address);
			circuitRenderManager.emplace(environment, this->circuitId, viewportId);
			toolManager.setCircuit(circuitId);
			viewManager.setCircuit(circuitId);
			dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
			dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
		}
	}
}

void CircuitView::setSimulator(const EvalLogicSimulator* simulator, const Address& address) {
	if (simulator == nullptr) {
		this->simulatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportSimulator(viewportId, nullptr, Address());
		toolManager.setCircuit(0);
		viewManager.setCircuit(0);
		dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else if (backend.getSimulatorManager().getSimulator(simulator->getSimulatorId()) != simulator) {
		logError("When setting CircuitView's simulator, a simulator with a different backend. Failed to connect! Doing nothing!", "CircuitView");
	} else {
		circuit_id_t circuitId = simulator->getCircuitId(address);
		Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId); // ok if null
		if (circuit == nullptr){
			logError("When setting CircuitView's simulator, failed to find circuit for simulator with circuit id {}", "CircuitView", circuitId);
			return;
		}
		this->simulatorId = simulator->getSimulatorId();
		this->address = address;
		this->circuitId = circuitId;
		circuitRenderManager.emplace(environment, circuitId, viewportId);
		MainRenderer::get().setViewportSimulator(viewportId, simulator, address);
		toolManager.setCircuit(circuitId);
		viewManager.setCircuit(circuitId);
		dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	}
}

void CircuitView::setCircuit(circuit_id_t circuitId) {
	if (backend.getCircuitManager().getCircuit(circuitId) == nullptr) {
		this->simulatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportSimulator(viewportId, nullptr, Address());
		toolManager.setCircuit(0);
		viewManager.setCircuit(0);
		dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else {
		this->simulatorId = 0;
		this->circuitId = circuitId;
		circuitRenderManager.emplace(environment, circuitId, viewportId);
		MainRenderer::get().setViewportSimulator(viewportId, nullptr, Address());
		toolManager.setCircuit(circuitId);
		viewManager.setCircuit(circuitId);
		dataUpdateEventManager.sendEvent("circuitViewChangeSimulator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	}
}

void CircuitView::viewChanged() {
	eventRegister.doEvent(PositionEvent("Pointer Move", viewManager.getPointerPosition()));
	eventRegister.doEvent(PositionEvent("ViewCenterMove", viewManager.getViewCenter()));
	eventRegister.doEvent(VectorEvent("ViewSizeChange", FVector(viewManager.getViewWidth(), viewManager.getViewHeight())));
	MainRenderer::get().moveViewportView(viewportId, viewManager.getTopLeft(), viewManager.getBottomRight());
}
