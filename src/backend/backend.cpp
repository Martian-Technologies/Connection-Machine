#include "backend.h"

#include "backend/circuitTests/circuitTestGroupManager.h"
#include "backend/wasm/wasm.h"
#include "backend/proceduralCircuits/wasmProceduralCircuit.h"

class CircuitFileManager;

Backend::Backend(CircuitFileManager& fileManager) :
	circuitManager(dataUpdateEventManager, simulatorManager, fileManager), simulatorManager(circuitManager, dataUpdateEventManager), circuitTestGroupManager(dataUpdateEventManager)
{
	logInfo("Initializing Backend", "Backend");
	Wasm::initialize();
	logInfo("Backend initialized", "Backend");
}

circuit_id_t Backend::createCircuit(const std::string& name, const std::string& uuid) {
	return circuitManager.createNewCircuit(name, uuid);
}

std::optional<simulator_id_t> Backend::createSimulator(circuit_id_t circuitId) {
	if (circuitManager.getCircuit(circuitId)) {
		return simulatorManager.createNewSimulator(circuitId);
	}
	return std::nullopt;
}

EvalLogicSimulator* Backend::getSimulator(simulator_id_t simulator_id) {
	return simulatorManager.getSimulator(simulator_id);
}

nlohmann::json Backend::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	stateJson["clipboardEditCounter"] = clipboardEditCounter;
	stateJson["clipboard"] = clipboard ? clipboard->dumpState() : nullptr;
	stateJson["circuitManager"] = circuitManager.dumpState();
	stateJson["simulatorManager"] = simulatorManager.dumpState();
	return stateJson;
}