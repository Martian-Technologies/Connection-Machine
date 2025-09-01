#include "backend.h"

#include "backend/proceduralCircuits/wasmProceduralCircuit.h"

class CircuitFileManager;

Backend::Backend(CircuitFileManager* fileManager) : circuitManager(&dataUpdateEventManager, &evaluatorManager, fileManager), evaluatorManager(&dataUpdateEventManager) {
	Wasm::initialize();
	circuitManager.connectListener(&evaluatorManager, std::bind(&EvaluatorManager::applyDiff, &evaluatorManager, std::placeholders::_1, std::placeholders::_2), 0);
}

circuit_id_t Backend::createCircuit(const std::string& name, const std::string& uuid) {
	return circuitManager.createNewCircuit(name, uuid);
}

std::optional<evaluator_id_t> Backend::createEvaluator(circuit_id_t circuitId) {
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (circuit) {
		return evaluatorManager.createNewEvaluator(circuitManager, circuitId);
	}
	return std::nullopt;
}

SharedCircuit Backend::getCircuit(circuit_id_t circuitId) {
	return circuitManager.getCircuit(circuitId);
}

SharedEvaluator Backend::getEvaluator(evaluator_id_t evaluatorId) {
	return evaluatorManager.getEvaluator(evaluatorId);
}
