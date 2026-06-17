#ifndef busReplacerEvalLayer_h
#define busReplacerEvalLayer_h

#include "baseEvalLayer.h"

class BusReplacerEvalLayer : public BaseEvalLayer {
	struct BusData {
		std::unordered_map<connection_end_id_t, std::vector<unsigned int>> connectionEnds;
		std::unordered_map<unsigned int, eval_gate_id> junctions;
	};
public:
	BusReplacerEvalLayer(EvalLayerState& currentState, const CircuitManager& circuitManager) : BaseEvalLayer(currentState, circuitManager) {}
	void run() override final;
	std::variant<EvalConnectionPoint, VecEvalConnectionPoint> getEvalConnectionPointsForConnectionPoint(EvalConnectionPoint connectionPoint) const;

private:
	IdMap<eval_gate_id, BusData> busses;
	IdMap<eval_gate_id, std::vector<eval_gate_id>> busJunctions;
};

#endif /* busReplacerEvalLayer_h */
