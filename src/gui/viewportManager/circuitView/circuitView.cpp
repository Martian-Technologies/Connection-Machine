#include "circuitView.h"

#include "backend/backend.h"
#include "environment/environment.h"
#include "events/customEvents.h"

#include "gpu/mainRenderer.h"

CircuitView::CircuitView(Environment& environment, ViewportId viewportId) :
	backend(environment.getBackend()), dataUpdateEventManager(environment.getBackend().getDataUpdateEventManager()), viewportId(viewportId),
	toolManager(environment, &eventRegister, viewportId, this), tutorialManager(environment, *this) {
	MainRenderer::get().moveViewportView(viewportId, viewManager.getTopLeft(), viewManager.getBottomRight());
	viewManager.setUpEvents(eventRegister);
	viewManager.connectViewChanged(std::bind(&CircuitView::viewChanged, this));
    //tutorial = std::make_unique<Tutorial>();
}

Circuit* CircuitView::getCircuit() { return backend.getCircuit(circuitId).get(); }
const Circuit* CircuitView::getCircuit() const { return backend.getCircuit(circuitId).get(); }

Evaluator* CircuitView::getEvaluator() { return backend.getEvaluator(evaluatorId).get(); }
const Evaluator* CircuitView::getEvaluator() const { return backend.getEvaluator(evaluatorId).get(); }

void CircuitView::setEvaluator(evaluator_id_t evaluatorId, const Address& address) {
	if (evaluatorId == 0) {
		this->evaluatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
		toolManager.setCircuit(nullptr);
		viewManager.setCircuit(nullptr);
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else {
		SharedEvaluator evaluator = backend.getEvaluatorManager().getEvaluator(evaluatorId);
		if (evaluator == nullptr) {
			logError("When setting CircuitView's evaluator, a evaluator with a different backend. Failed to connect! Doing nothing!", "CircuitView");
		} else {
			this->evaluatorId = evaluatorId;
			this->address = address;
			circuit_id_t circuitId = evaluator->getCircuitId(address);
			SharedCircuit circuit = backend.getCircuit(circuitId); // ok if null
			this->circuitId = circuit->getCircuitId();
			circuitRenderManager.emplace(backend, this->circuitId, viewportId);
			MainRenderer::get().setViewportEvaluator(viewportId, evaluator.get(), address);
			toolManager.setCircuit(circuit.get());
			viewManager.setCircuit(circuit.get());
			dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
			dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
		}
	}
}

void CircuitView::setEvaluator(std::shared_ptr<Evaluator> evaluator, const Address& address) {
	if (evaluator == nullptr) {
		this->evaluatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
		toolManager.setCircuit(nullptr);
		viewManager.setCircuit(nullptr);
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else if (backend.getEvaluatorManager().getEvaluator(evaluator->getEvaluatorId()) != evaluator) {
		logError("When setting CircuitView's evaluator, a evaluator with a different backend. Failed to connect! Doing nothing!", "CircuitView");
	} else {
		this->evaluatorId = evaluator->getEvaluatorId();
		this->address = address;
		circuit_id_t circuitId = evaluator->getCircuitId(address);
		SharedCircuit circuit = backend.getCircuit(circuitId); // ok if null
		this->circuitId = circuit->getCircuitId();
		circuitRenderManager.emplace(backend, circuit->getCircuitId(), viewportId);
		MainRenderer::get().setViewportEvaluator(viewportId, evaluator.get(), address);
		toolManager.setCircuit(circuit.get());
		viewManager.setCircuit(circuit.get());
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	}
}

void CircuitView::setCircuit(circuit_id_t circuitId) {
	if (circuitId == 0) {
		this->evaluatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
		toolManager.setCircuit(nullptr);
		viewManager.setCircuit(nullptr);
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else {
		SharedCircuit circuit = backend.getCircuit(circuitId);
		if (circuit == nullptr) {
			logError("When setting CircuitView's circuit, a circuit with a different backend. Failed to connect! Doing nothing!", "CircuitView");
		} else {
			this->evaluatorId = 0;
			this->circuitId = circuit->getCircuitId();
			circuitRenderManager.emplace(backend, circuit->getCircuitId(), viewportId);
			MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
			toolManager.setCircuit(circuit.get());
			viewManager.setCircuit(circuit.get());
			dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
			dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
		}
	}
}

void CircuitView::setCircuit(SharedCircuit circuit) {
	if (circuit == nullptr) {
		this->evaluatorId = 0;
		this->circuitId = 0;
		circuitRenderManager.reset();
		MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
		toolManager.setCircuit(nullptr);
		viewManager.setCircuit(nullptr);
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	} else if (backend.getCircuit(circuit->getCircuitId()) != circuit) {
		logError("When setting CircuitView's circuit, a circuit with a different backend. Failed to connect! Doing nothing!", "CircuitView");
	} else {
		this->evaluatorId = 0;
		this->circuitId = circuit->getCircuitId();
		circuitRenderManager.emplace(backend, circuit->getCircuitId(), viewportId);
		MainRenderer::get().setViewportEvaluator(viewportId, nullptr, Address());
		toolManager.setCircuit(circuit.get());
		viewManager.setCircuit(circuit.get());
		dataUpdateEventManager.sendEvent("circuitViewChangeEvaluator", this);
		dataUpdateEventManager.sendEvent("circuitViewChangeCircuit", this);
	}
}

void CircuitView::viewChanged() {
	eventRegister.doEvent(PositionEvent("Pointer Move", viewManager.getPointerPosition()));
	MainRenderer::get().moveViewportView(viewportId, viewManager.getTopLeft(), viewManager.getBottomRight());
}
