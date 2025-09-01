#ifndef backend_h
#define backend_h

#include "evaluator/evaluatorManager.h"
#include "dataUpdateEventManager.h"
#include "circuit/circuitManager.h"
#include "container/copiedBlocks.h"
#include "util/uuid.h"

class Backend {
public:
	Backend(CircuitFileManager* fileManager);

	// Creates a new Circuit. Returns circuit_id_t.
	circuit_id_t createCircuit() { return circuitManager.createNewCircuit(); }
	circuit_id_t createCircuit(const std::string& name, const std::string& uuid = generate_uuid_v4());
	// Attempts to create a Evaluator for a Circuit. Returns evaluator_id_t if successful.
	std::optional<evaluator_id_t> createEvaluator(circuit_id_t circuitId);

	inline BlockDataManager* getBlockDataManager() { return getCircuitManager().getBlockDataManager(); }

	inline CircuitManager& getCircuitManager() { return circuitManager; }
	inline const CircuitManager& getCircuitManager() const { return circuitManager; }

	inline const EvaluatorManager& getEvaluatorManager() const { return evaluatorManager; }

	inline DataUpdateEventManager* getDataUpdateEventManager() { return &dataUpdateEventManager; }

	SharedCircuit getCircuit(circuit_id_t circuitId);
	SharedEvaluator getEvaluator(evaluator_id_t evaluatorId);

	const SharedCopiedBlocks getClipboard() const { return clipboard; }
	unsigned long long getClipboardEditCounter() const { return clipboardEditCounter; }
	void setClipboard(SharedCopiedBlocks copiedBlocks) { clipboard = copiedBlocks; ++clipboardEditCounter; }

private:
	SharedCopiedBlocks clipboard = nullptr;
	unsigned long long clipboardEditCounter = 1;

	DataUpdateEventManager dataUpdateEventManager; // this needs to be constructed first
	CircuitManager circuitManager;
	EvaluatorManager evaluatorManager;
};

#endif /* backend_h */
