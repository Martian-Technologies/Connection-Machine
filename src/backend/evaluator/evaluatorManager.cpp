#include "evaluatorManager.h"

#include "backend/circuit/circuitManager.h"
#include "evaluator.h"

evaluator_id_t EvaluatorManager::createNewEvaluator(CircuitManager& circuitManager, circuit_id_t circuitId) {
	evaluator_id_t id = getNewEvaluatorId();
	evaluators.emplace(
		id,
		std::make_shared<Evaluator>(id, circuitManager, *circuitManager.getBlockDataManager(), *circuitManager.getCircuitBlockDataManager(), circuitId, dataUpdateEventManager)
	);
	dataUpdateEventManager->sendEvent("addressTreeMakeBranch");
	return id;
}

void EvaluatorManager::applyDiff(DifferenceSharedPtr difference, circuit_id_t circuitId) {
	for (auto& [id, evaluator] : evaluators) {
		evaluator->makeEdit(difference, circuitId);
	}
}
