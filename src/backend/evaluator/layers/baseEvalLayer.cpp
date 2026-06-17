#include "baseEvalLayer.h"

#include "evalLayerState.h"

BaseEvalLayer::BaseEvalLayer(EvalLayerState& currentState, const CircuitManager& circuitManager) :
	currentState(currentState), nextState(currentState.getOrMakeNextLayerState()), circuitManager(circuitManager) {}

EvalConnectionPoint BaseEvalLayer::getMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint) const {
	auto connectionPointIter = nextState.getConnectionPointRemapping().find(evalConnectionPoint);
	if (connectionPointIter != nextState.getConnectionPointRemapping().end()) {
		return connectionPointIter->second;
	}
	auto evalGateIdIter = nextState.getGateIdRemapping().find(evalConnectionPoint.gateId);
	if (evalGateIdIter != nextState.getGateIdRemapping().end()) {
		return EvalConnectionPoint(evalGateIdIter->second, evalConnectionPoint.connectionEndId);
	}
	auto toNothingIter = nextState.getConnectionPointRemappingToNothing().find(evalConnectionPoint);
	if (toNothingIter == nextState.getConnectionPointRemappingToNothing().end()) {
		logError("Could not find mapping for evalConnectionPoint {}.", "BaseEvalLayer::getMappedEvalConnectionPoint", evalConnectionPoint);
	}
	return EvalConnectionPoint::null();
}

VecEvalConnectionPoint BaseEvalLayer::getReversedMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint) const {
	VecEvalConnectionPoint evalConnectionPoints;
	auto connectionPointIterPair = nextState.getConnectionPointReverseRemapping().equal_range(evalConnectionPoint);
	for (auto iter = connectionPointIterPair.first; iter != connectionPointIterPair.second; iter++) {
		evalConnectionPoints.push_back(iter->second);
	}
	auto evalGateIdIterPair = nextState.getGateIdReverseRemapping().equal_range(evalConnectionPoint.gateId);
	for (auto iter = evalGateIdIterPair.first; iter != evalGateIdIterPair.second; iter++) {
		evalConnectionPoints.emplace_back(iter->second, evalConnectionPoint.connectionEndId);
	}
	assert(nextState.getGate(evalConnectionPoint.gateId));
	for (EvalConnectionPoint gateId : evalConnectionPoints) assert(currentState.getGate(gateId.gateId));
	return evalConnectionPoints;
}

void BaseEvalLayer::getReversedMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint, VecEvalConnectionPoint& evalConnectionPoints) const {
	auto connectionPointIterPair = nextState.getConnectionPointReverseRemapping().equal_range(evalConnectionPoint);
	for (auto iter = connectionPointIterPair.first; iter != connectionPointIterPair.second; iter++) {
		evalConnectionPoints.push_back(iter->second);
	}
	auto evalGateIdIterPair = nextState.getGateIdReverseRemapping().equal_range(evalConnectionPoint.gateId);
	for (auto iter = evalGateIdIterPair.first; iter != evalGateIdIterPair.second; iter++) {
		evalConnectionPoints.emplace_back(iter->second, evalConnectionPoint.connectionEndId);
	}
	assert(nextState.getGate(evalConnectionPoint.gateId));
}
