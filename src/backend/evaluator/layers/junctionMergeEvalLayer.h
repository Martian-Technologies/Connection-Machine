#ifndef junctionMergeEvalLayer_h
#define junctionMergeEvalLayer_h

#include "backend/evaluator/evalDefs.h"
#include "baseEvalLayer.h"

namespace EvalConnectionEndInfo {
	bool isConnectionEndIdSinglePin(EvalGateType gateType, connection_end_id_t connectionEndId);
	bool isOutputConnectionPort(EvalGateType gateType, connection_end_id_t connectionEndId);
}

class JunctionMergeEvalLayer : public BaseEvalLayer {
public:
	JunctionMergeEvalLayer(EvalLayerState& currentState, const CircuitManager& circuitManager) : BaseEvalLayer(currentState, circuitManager) {}
	void run() override final;

private:
	std::tuple<
		std::vector<eval_gate_id>,
		std::unordered_set<EvalConnectionPoint>,
		std::unordered_map<EvalConnectionPoint, unsigned int>,
		EvalGateType
	> gatherJunctionGroup(EvalConnectionPoint connectionPointToScanFrom, const EvalLayerState& evalLayerState) const;

	std::unordered_map<EvalConnectionPoint, eval_gate_id> connectionPointRemapping;
	IdMultiMap<eval_gate_id, EvalConnectionPoint> connectionPointReverseRemapping;
};

#endif /* junctionMergeEvalLayer_h */
