#include "layerRunner.h"

#include "evalLayerState.h"
#include "subcircuitEvalLayer.h"
#include "junctionMergeEvalLayer.h"
#include "switchReplacerEvalLayer.h"
#include "busReplacerEvalLayer.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

LayerRunner::LayerRunner(IdProvider<eval_gate_id>& evalGateIdProvider, Evaluator& evaluator, const CircuitManager& circuitManager) {
	evalTopLayerState = std::make_unique<EvalLayerState>(evalGateIdProvider);
	layers.emplace_back(std::make_unique<SubcircuitEvalLayer>(*evalTopLayerState, evaluator, circuitManager));
	layers.emplace_back(std::make_unique<SwitchReplacerEvalLayer>(layers.back()->getNextState(), circuitManager));
	layers.emplace_back(std::make_unique<JunctionMergeEvalLayer>(layers.back()->getNextState(), circuitManager));
	busLayerIndex = layers.size();
	layers.emplace_back(std::make_unique<BusReplacerEvalLayer>(layers.back()->getNextState(), circuitManager));
	layers.emplace_back(std::make_unique<JunctionMergeEvalLayer>(layers.back()->getNextState(), circuitManager));
	assert(evalTopLayerState);
}

LayerRunner::~LayerRunner() = default;

void LayerRunner::runAll() {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	// logInfo("------------------------------------------------");
	// evalTopLayerState->visualize();
	// unsigned int index = 0;
	for (std::unique_ptr<BaseEvalLayer>& layer : layers) {
		// if (index < 4) {
		// 	logInfo("-- {} --", "", index);
		// 	layer->getNextState().visualize();
		// }
		layer->run();
		// if (index < 4) {
		// 	layer->getNextState().visualize();
		// }
		// index++;
	}
}

void LayerRunner::resetEdits() {
	evalTopLayerState->resetEdits();
	for (std::unique_ptr<BaseEvalLayer>& layer : layers) {
		layer->getNextState().resetEdits();
	}
}

EvalLayerState& LayerRunner::getInputLayer() { return *evalTopLayerState; }

const EvalLayerState& LayerRunner::getInputLayer() const { return *evalTopLayerState; }

const EvalLayerState& LayerRunner::getOutputLayer() const { return layers.back()->getNextState(); }
const EvalLayerState& LayerRunner::getOutputLayerForOtherEvals() const { return layers[busLayerIndex - 1]->getNextState(); }

std::vector<std::pair<eval_gate_id, circuit_id_t>> LayerRunner::getSubcircuits() const {
	return dynamic_cast<SubcircuitEvalLayer*>(layers[0].get())->getSubcircuits();
}

std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> LayerRunner::getMappedAddress(eval_gate_id gateId, const Address& address) const {
	EvalConnectionPoint evalConnectionPoint = dynamic_cast<SubcircuitEvalLayer*>(layers[0].get())->getMappedAddress(gateId, address);
	if (evalConnectionPoint.isNull()) return EvalConnectionPoint::null();
	for (unsigned int i = 1; i < busLayerIndex; i++) {
		evalConnectionPoint = layers[i]->getMappedEvalConnectionPoint(evalConnectionPoint);
	}
	const BusReplacerEvalLayer* busReplacerEvalLayer = dynamic_cast<const BusReplacerEvalLayer*>(layers[busLayerIndex].get());
	assert(busReplacerEvalLayer);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPointVariant = busReplacerEvalLayer->getEvalConnectionPointsForConnectionPoint(evalConnectionPoint);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPointVariant)) {
		return layers.back()->getMappedEvalConnectionPoint(std::get<EvalConnectionPoint>(evalConnectionPointVariant));
	} else {
		std::vector<EvalConnectionPoint> vec;
		for (EvalConnectionPoint connectionPoint : std::get<std::vector<EvalConnectionPoint>>(evalConnectionPointVariant)) {
			vec.push_back(layers.back()->getMappedEvalConnectionPoint(connectionPoint));
		}
		return vec;
	}
}

EvalConnectionPoint LayerRunner::getMappedAddressForOtherEvals(eval_gate_id gateId, const Address& address) const {
	EvalConnectionPoint evalConnectionPoint = dynamic_cast<SubcircuitEvalLayer*>(layers[0].get())->getMappedAddress(gateId, address);
	if (evalConnectionPoint.isNull()) return EvalConnectionPoint::null();
	for (unsigned int i = 1; i < busLayerIndex; i++) { // skip bus stuff
		evalConnectionPoint = layers[i]->getMappedEvalConnectionPoint(evalConnectionPoint);
	}
	return evalConnectionPoint;
}

std::vector<std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>>> LayerRunner::getMappedConnectionPointsFromBusLayer(const VecEvalConnectionPoint& evalConnectionPoints) const {
	std::vector<std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>>> evalConnectionPointsTmp;
	const BusReplacerEvalLayer* busReplacerEvalLayer = dynamic_cast<const BusReplacerEvalLayer*>(layers[busLayerIndex].get());
	assert(busReplacerEvalLayer);
	for (EvalConnectionPoint connectionPoint : evalConnectionPoints) {
		evalConnectionPointsTmp.push_back(busReplacerEvalLayer->getEvalConnectionPointsForConnectionPoint(connectionPoint));
	}
	std::vector<std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>>> outputConnectionPoints;
	for (std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPointVariant : evalConnectionPointsTmp) {
		if (std::holds_alternative<EvalConnectionPoint>(connectionPointVariant)) {
			outputConnectionPoints.push_back(layers.back()->getMappedEvalConnectionPoint(std::get<EvalConnectionPoint>(connectionPointVariant)));
		} else {
			outputConnectionPoints.push_back(std::vector<EvalConnectionPoint>());
			std::vector<EvalConnectionPoint>& vec = std::get<std::vector<EvalConnectionPoint>>(outputConnectionPoints.back());
			for (EvalConnectionPoint connectionPoint : std::get<std::vector<EvalConnectionPoint>>(connectionPointVariant)) {
				vec.push_back(layers.back()->getMappedEvalConnectionPoint(connectionPoint));
			}
		}
	}
	return outputConnectionPoints;
}

VecVecEvalConnectionPoint LayerRunner::getReversedMappedConnectionPointsWithAddressForOtherEvals(const VecEvalConnectionPoint& evalConnectionPoints, eval_gate_id gateId, const Address& address) const {
	VecVecEvalConnectionPoint evalConnectionPointVecs;
	for (EvalConnectionPoint connectionPoint : evalConnectionPoints) {
		evalConnectionPointVecs.push_back({});
		layers[busLayerIndex - 1]->getReversedMappedEvalConnectionPoint(connectionPoint, evalConnectionPointVecs.back());
	}
	VecVecEvalConnectionPoint lastSimulatorConnectionPointVecs;
	for (unsigned int i = busLayerIndex - 2; i >= 1; i--) {
		lastSimulatorConnectionPointVecs = std::move(evalConnectionPointVecs);
		evalConnectionPointVecs.clear();
		for (VecEvalConnectionPoint vec : lastSimulatorConnectionPointVecs) {
			evalConnectionPointVecs.push_back({});
			for (EvalConnectionPoint connectionPoint : vec) {
				layers[i]->getReversedMappedEvalConnectionPoint(connectionPoint, evalConnectionPointVecs.back());
			}
		}
	}
	return dynamic_cast<SubcircuitEvalLayer*>(layers[0].get())->getReversedMappedConnectionPointGroupsWithAddress(evalConnectionPointVecs, gateId, address);
}
VecEvalConnectionPoint LayerRunner::getReversedMappedConnectionPointsAboveBusLayer(const VecEvalConnectionPoint& evalConnectionPoints) const {
	VecEvalConnectionPoint evalConnectionPointsTmp;
	for (EvalConnectionPoint connectionPoint : evalConnectionPoints) {
		layers.back()->getReversedMappedEvalConnectionPoint(connectionPoint, evalConnectionPointsTmp);
	}
	VecEvalConnectionPoint outputConnectionPoints;
	for (EvalConnectionPoint connectionPoint : evalConnectionPointsTmp) {
		layers[busLayerIndex]->getReversedMappedEvalConnectionPoint(connectionPoint, outputConnectionPoints);
	}
	return outputConnectionPoints;
}
VecVecEvalConnectionPoint LayerRunner::getReversedMappedConnectionPointGroupsWithAddressForOtherEvals(VecVecEvalConnectionPoint evalConnectionPoints, eval_gate_id gateId, const Address& address) const {
	VecVecEvalConnectionPoint lastSimulatorConnectionPoints;
	for (unsigned int i = busLayerIndex - 1; i >= 1; i--) {
		lastSimulatorConnectionPoints = std::move(evalConnectionPoints);
		evalConnectionPoints.clear();
		for (VecEvalConnectionPoint vec : lastSimulatorConnectionPoints) {
			evalConnectionPoints.push_back({});
			for (EvalConnectionPoint connectionPoint : vec) {
				layers[i]->getReversedMappedEvalConnectionPoint(connectionPoint, evalConnectionPoints.back());
			}
		}
	}
	return dynamic_cast<SubcircuitEvalLayer*>(layers[0].get())->getReversedMappedConnectionPointGroupsWithAddress(evalConnectionPoints, gateId, address);
}

std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> LayerRunner::getMappedEvalConnectionPoint(EvalConnectionPoint evalConnectionPoint) const {
	for (unsigned int i = 0; i < busLayerIndex; i++) {
		if (evalConnectionPoint.isNull()) return EvalConnectionPoint::null();
		evalConnectionPoint = layers[i]->getMappedEvalConnectionPoint(evalConnectionPoint);
	}
	const BusReplacerEvalLayer* busReplacerEvalLayer = dynamic_cast<const BusReplacerEvalLayer*>(layers[busLayerIndex].get());
	assert(busReplacerEvalLayer);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPoints = busReplacerEvalLayer->getEvalConnectionPointsForConnectionPoint(evalConnectionPoint);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPoints)) {
		return layers.back()->getMappedEvalConnectionPoint(std::get<EvalConnectionPoint>(evalConnectionPoints));
	}
	std::vector<EvalConnectionPoint> evalConnectionPointsMapped;
	for (EvalConnectionPoint connectionPoint : std::get<std::vector<EvalConnectionPoint>>(std::move(evalConnectionPoints))) {
		EvalConnectionPoint mappedPoint = layers.back()->getMappedEvalConnectionPoint(connectionPoint);
		if (mappedPoint.isNull()) continue;
		evalConnectionPointsMapped.push_back(mappedPoint);
	}
	return evalConnectionPointsMapped;
}

EvalConnectionPoint LayerRunner::getMappedEvalConnectionPointForOtherEvals(EvalConnectionPoint evalConnectionPoint) const {
	for (unsigned int i = 0; i < busLayerIndex; i++) {
		if (evalConnectionPoint.isNull()) return EvalConnectionPoint::null();
		evalConnectionPoint = layers[i]->getMappedEvalConnectionPoint(evalConnectionPoint);
	}
	return evalConnectionPoint;
}

void LayerRunner::getReversedMappedEvalConnectionPointForOtherEvals(EvalConnectionPoint evalConnectionPoint, VecEvalConnectionPoint& outputVector) const {
	VecEvalConnectionPoint evalConnectionPoints = { evalConnectionPoint };
	VecEvalConnectionPoint lastSimulatorConnectionPoints;
	for (unsigned int i = busLayerIndex - 1; i >= 1; i--) {
		lastSimulatorConnectionPoints = std::move(evalConnectionPoints);
		evalConnectionPoints.clear();
		for (EvalConnectionPoint point : lastSimulatorConnectionPoints) {
			layers[i]->getReversedMappedEvalConnectionPoint(point, evalConnectionPoints);
		}
	}
	for (EvalConnectionPoint point : evalConnectionPoints) {
		dynamic_cast<const SubcircuitEvalLayer*>(layers[0].get())->getReversedMappedEvalConnectionPoint(point, outputVector);
	}
}
