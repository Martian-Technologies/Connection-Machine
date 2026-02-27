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
	LinkedGateGroup(
		std::vector<SimulatorGate> gates,
		std::vector<std::pair<gate_group_id_t, std::vector<w_vec_index>>> pullConnectionPointsByGroup,
		std::vector<EvalConnectionPoint> pushConnectionPoints
	) : gates(std::move(gates)),
		pullConnectionPointsByGroup(std::move(pullConnectionPointsByGroup)),
		pushConnectionPoints(std::move(pushConnectionPoints)) {}

	std::string toString() const {
		std::string result = "LinkedGateGroup:\n";
		for (const auto& gate : gates) {
			result += "\t" + blocktype_to_string(gate.type) + "(" + std::to_string(gate.id) + "), ";
		}
		result += "\npullConnectionPointsByGroup:\n";
		for (const auto& [groupId, pullConnectionPoints] : pullConnectionPointsByGroup) {
			result += "\tGroup " + std::to_string(groupId) + ": ";
			for (const auto& pullConnectionPoint : pullConnectionPoints) {
				result += fmt::to_string(pullConnectionPoint) + ", ";
			}
			result += "\n";
		}
		result += "pushConnectionPoints:\n";
		for (const auto& pushConnectionPoint : pushConnectionPoints) {
			result += "\t" + fmt::to_string(pushConnectionPoint) + ", ";
		}
		result += "\n";
		return result;
	}
	std::vector<SimulatorGate> gates;
	std::vector<std::pair<gate_group_id_t, std::vector<w_vec_index>>> pullConnectionPointsByGroup;
	std::vector<EvalConnectionPoint> pushConnectionPoints;
};

#endif /* compiledGateGroup_h */