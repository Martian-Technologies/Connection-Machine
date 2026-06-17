#ifndef backend_h
#define backend_h

#include "backend/evaluator/simulatorManager.h"
#include "backend/circuitTests/circuitTestGroupManager.h"
#include "evaluator/simulator/simulatorDefs.h"
#include "dataUpdateEventManager.h"
#include "circuit/circuitManager.h"
#include "container/copiedBlocks.h"
#include "util/uuid.h"


class Backend {
public:
	Backend(CircuitFileManager& fileManager);
	Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

	// Creates a new Circuit. Returns circuit_id_t.
	circuit_id_t createCircuit() { return circuitManager.createNewCircuit(); }
	circuit_id_t createCircuit(const std::string& name, const std::string& uuid = generate_uuid_v4());
	// Attempts to create a Simulator for a Circuit. Returns simulator_id_t if successful.
	std::optional<simulator_id_t> createSimulator(circuit_id_t circuitId);

	inline BlockDataManager& getBlockDataManager() { return getCircuitManager().getBlockDataManager(); }

	inline CircuitManager& getCircuitManager() { return circuitManager; }
	inline const CircuitManager& getCircuitManager() const { return circuitManager; }

	inline SimulatorManager& getSimulatorManager() { return simulatorManager; }
	inline const SimulatorManager& getSimulatorManager() const { return simulatorManager; }

	inline CircuitTestGroupManager& getCircuitTestGroupManager() { return circuitTestGroupManager; }
	inline const CircuitTestGroupManager& getCircuitTestGroupManager() const { return circuitTestGroupManager; }

	inline DataUpdateEventManager& getDataUpdateEventManager() { return dataUpdateEventManager; }

	EvalLogicSimulator* getSimulator(simulator_id_t simulatorId);

	const SharedCopiedBlocks getClipboard() const { return clipboard; }
	unsigned long long getClipboardEditCounter() const { return clipboardEditCounter; }
	void setClipboard(SharedCopiedBlocks copiedBlocks) { clipboard = copiedBlocks; ++clipboardEditCounter; }

	nlohmann::json dumpState() const;

private:
	SharedCopiedBlocks clipboard = nullptr;
	unsigned long long clipboardEditCounter = 1;

	DataUpdateEventManager dataUpdateEventManager; // this needs to be constructed first
	CircuitManager circuitManager;
	SimulatorManager simulatorManager;
	CircuitTestGroupManager circuitTestGroupManager;
};

#endif /* backend_h */
