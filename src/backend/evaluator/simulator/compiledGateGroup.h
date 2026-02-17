#ifndef compiledGateGroup_h
#define compiledGateGroup_h

#include "simulatorDefs.h"

class CompiledGateGroup {
public:
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
			result += "\t" + blocktype_to_string(gate.type) + ", ";
		}
		result += "\npullConnectionPoints:\n";
		for (const auto& pullConnectionPoint : pullConnectionPoints) {
			result += "\t" + fmt::to_string(pullConnectionPoint) + ", ";
		}
		result += "\n";
		return result;
	}

private:
	std::vector<SimulatorGate> gates;
	std::vector<EvalConnectionPoint> pullConnectionPoints;
};

#endif /* compiledGateGroup_h */