#ifndef compiledGateGroup_h
#define compiledGateGroup_h

#include "simulatorDefs.h"

class CompiledGateGroup {
public:
	CompiledGateGroup(
		std::vector<SimulatorGate> gates,
		std::vector<EvalConnectionPoint> pullConnectionPoints,
		std::vector<EvalConnectionPoint> publishConnectionPoints
	) : gates(std::move(gates)),
		pullConnectionPoints(std::move(pullConnectionPoints)),
		publishConnectionPoints(std::move(publishConnectionPoints)) {};
	~CompiledGateGroup() = default;
	bool operator==(const CompiledGateGroup& o) const = default;

private:
	std::vector<SimulatorGate> gates;
	std::vector<EvalConnectionPoint> pullConnectionPoints;
	std::vector<EvalConnectionPoint> publishConnectionPoints;
};

#endif /* compiledGateGroup_h */