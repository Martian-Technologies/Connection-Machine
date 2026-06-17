#ifndef subcircuitEvalLayer_h
#define subcircuitEvalLayer_h

#include "backend/circuit/circuitDefs.h"
#include "baseEvalLayer.h"

class CircuitManager;
class EvalLayerState;
class Address;

typedef std::vector<std::vector<EvalConnectionPoint>> VecVecEvalConnectionPoint;
typedef std::vector<EvalConnectionPoint> VecEvalConnectionPoint;

class SubcircuitEvalLayer : public BaseEvalLayer {
public:
	struct SubcircuitData {
		SubcircuitData(circuit_id_t circuitId, const EvalLayerState& outputEvalLayer) : circuitId(circuitId), outputEvalLayer(outputEvalLayer) { }
		circuit_id_t circuitId;
		const EvalLayerState& outputEvalLayer;
		IdMap<eval_gate_id, eval_gate_id> otherSimulatorToThisSimulatorIdMapping;
		IdMap<eval_gate_id, eval_gate_id> thisSimulatorIdMappingToOtherSimulator;
	};
	SubcircuitEvalLayer(EvalLayerState& currentState, Evaluator& evaluator, const CircuitManager& circuitManager) :
		BaseEvalLayer(currentState, circuitManager), evaluator(evaluator) { }
	void run() override final;

	std::vector<std::pair<eval_gate_id, circuit_id_t>> getSubcircuits() const;

	EvalConnectionPoint getMappedAddress(eval_gate_id gateId, const Address& address) const;
	VecVecEvalConnectionPoint getReversedMappedConnectionPointGroupsWithAddress(
		const VecVecEvalConnectionPoint& connectionPoints,
		eval_gate_id gateId,
		const Address& address
	) const;
	void getReversedMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint, VecEvalConnectionPoint& evalConnectionPoints) const;

	// Only called by EvaluatorInternal
	void processICEdits(circuit_id_t circuitId, const std::vector<std::tuple<connection_end_id_t, EvalConnectionPoint, EvalConnectionPoint>>& updatedPortIds);
private:
	IdMap<eval_gate_id, SubcircuitData> subcircuits;
	Evaluator& evaluator;
};

#endif /* subcircuitEvalLayer_h */
