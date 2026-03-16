#ifndef compiledGateGroup_h
#define compiledGateGroup_h

#include "simulatorDefs.h"

struct CompiledGateGroup {
	CompiledGateGroup(
		std::vector<SimulatorGate> gates,
		std::vector<EvalConnectionPoint> pullConnectionPoints
	) : gates(std::move(gates)),
		pullConnectionPoints(std::move(pullConnectionPoints)) {};
	CompiledGateGroup() = default;
	~CompiledGateGroup() = default;
	bool operator==(const CompiledGateGroup& o) const = default;

	std::string toString() const {
		std::string result = "CompiledGateGroup:\n";
		for (const auto& gate : gates) {
			result += "\t" + blocktype_to_string(gate.type) + "(" + std::to_string(gate.id) + "), ";
		}
		result += "\npullConnectionPoints:\n";
		for (const auto& pullConnectionPoint : pullConnectionPoints) {
			result += "\t" + fmt::to_string(pullConnectionPoint) + ", ";
		}
		result += "\n";
		return result;
	}
	std::vector<SimulatorGate> gates;
	std::vector<EvalConnectionPoint> pullConnectionPoints;
};

struct LinkedGateGroup {
	LinkedGateGroup() = default;
	LinkedGateGroup(
		std::vector<SimulatorGate> gates,
		std::unordered_map<EvalConnectionPoint, std::pair<gate_group_id_t, unsigned int>> pullConnectionPoints,
		std::vector<EvalConnectionPoint> pushConnectionPoints
	) : gates(std::move(gates)),
		pullConnectionPoints(std::move(pullConnectionPoints)),
		pushConnectionPoints(std::move(pushConnectionPoints)) {}

	bool operator==(const LinkedGateGroup& o) const = default;

	std::string toString() const {
		std::string result = "LinkedGateGroup:\n";
		for (const auto& gate : gates) {
			result += "\t" + blocktype_to_string(gate.type) + "(" + to_string(gate.id) + "), ";
		}
		result += "\npullConnectionPoints:\n";
		for (const auto& [connectionPoint, groupAndIndices] : pullConnectionPoints) {
			result += "\tGroup " + to_string(groupAndIndices.first) + " index " + std::to_string(groupAndIndices.second) + ": ";
			result += fmt::to_string(connectionPoint) + ", ";
		}
		result += "\n";
		result += "pushConnectionPoints (calculated every tick):\n";
		for (const auto& pushConnectionPoint : pushConnectionPoints) {
			result += "\t" + fmt::to_string(pushConnectionPoint) + ", ";
		}
		result += "\n";
		return result;
	}
	std::vector<SimulatorGate> gates;
	std::unordered_map<EvalConnectionPoint, std::pair<gate_group_id_t, unsigned int>> pullConnectionPoints;
	std::vector<EvalConnectionPoint> pushConnectionPoints;
};

#endif /* compiledGateGroup_h */