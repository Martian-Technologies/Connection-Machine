#include "evalCircuit.h"

std::optional<CircuitNode> EvalCircuit::getNode(Position pos) const noexcept {
	const CircuitNode* node = circuitNodes.get(pos);
	if (node) {
		return *node;
	}
	return std::nullopt;
}

nlohmann::json EvalCircuit::dumpState() const {
	nlohmann::json stateJson;
	stateJson["id"] = id.get();
	stateJson["parentEvalId"] = parentEvalId.get();
	stateJson["circuitId"] = circuitId;
	stateJson["circuitNodes"] = circuitNodes.dumpStateAndInner();
	return stateJson;
}
