#include "evalLogicSimulator.h"

#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/layers/layerRunner.h"
#include "backend/evaluator/layers/evalLayerState.h"
#include "backend/evaluator/evaluatorInternal.h"
#include "backend/evaluator/evaluator.h"

EvalLogicSimulator::EvalLogicSimulator(
	simulator_id_t simulatorId,
	const CircuitManager& circuitManager,
	circuit_id_t circuitId,
	DataUpdateEventManager& dataUpdateEventManager
) : logicSimulator(simulatorId, dirtySimulatorIds, dataUpdateEventManager), circuitManager(circuitManager), circuitId(circuitId),
	evaluatorInternal(circuitManager.getCircuit(circuitId)->getEvaluator().getEvaluatorInternal()), simulatorId(simulatorId) {
	const Circuit* circuit = circuitManager.getCircuit(circuitId).get();
	assert(circuit);
	circuit->getEvaluator().addSimulator(*this);
	const EvalLayerState& evalLayerState = circuit->getEvaluator().getEvaluatorInternal().getLayerRunner().getOutputLayer();

	for (std::pair<eval_gate_id, EvalGate> pair : evalLayerState.getGates()) {
		logicSimulator.addGate(pair.first, getBlockType(pair.second.type));
	}
	for (std::pair<eval_gate_id, EvalGate> pair : evalLayerState.getGates()) {
		for (std::pair<connection_end_id_t, std::unordered_set<EvalConnectionPoint>> connectionsPair : pair.second.connections) {
			for (EvalConnectionPoint otherConnectionPoint : connectionsPair.second) {
				// need to add some logic to not double make connections
				if (
					(otherConnectionPoint.gateId > pair.second.gateId) ||
					(otherConnectionPoint.gateId == pair.second.gateId && otherConnectionPoint.connectionEndId.get() > connectionsPair.first.get())
				) {
					unsigned int weight = evalLayerState.getConnectionWeight(EvalConnection(
						EvalConnectionPoint(pair.first, connectionsPair.first),
						otherConnectionPoint
					));
					logicSimulator.addConnection(EvalConnection(
						EvalConnectionPoint(pair.first, connectionsPair.first),
						otherConnectionPoint
					), weight);
				}
			}
		}
	}
	logicSimulator.resetStates();
}

EvalLogicSimulator::~EvalLogicSimulator() { circuitManager.getCircuit(circuitId)->getEvaluator().removeSimulator(*this); }

std::string EvalLogicSimulator::getSimulatorName() const {
	return "Sim " + std::to_string(getSimulatorId()) + " (" + circuitManager.getCircuit(circuitId)->getCircuitNameNumber() + ")";
}

circuit_id_t EvalLogicSimulator::getCircuitId(const Address& address) const {
	return circuitManager.getCircuit(circuitId)->getCircuitId(address);
}

void EvalLogicSimulator::setState(const Address& address, logic_state_t state) {
	std::lock_guard lock(mux);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (!std::holds_alternative<EvalConnectionPoint>(connectionPoints)) return;
	std::optional<simulator_state_reference> simulatorStateIndex = getSimulatorStateIndex_noMux(std::get<EvalConnectionPoint>(connectionPoints));
	if (!simulatorStateIndex.has_value()) {
		logError("Could not find simulator state index for address: {}", "EvalLogicSimulator", address.toString());
		return;
	}
	setState(simulatorStateIndex.value(), state);
}

logic_state_t EvalLogicSimulator::getState(const Address& address) const {
	std::lock_guard lock(mux);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (!std::holds_alternative<EvalConnectionPoint>(connectionPoints)) return logic_state_t::UNDEFINED;
	simulator_state_reference simulatorStateIndex = getSimulatorStateIndex_noMux(std::get<EvalConnectionPoint>(connectionPoints)).value_or(3);
	return getState(simulatorStateIndex);
}

std::variant<logic_state_t, std::vector<logic_state_t>> EvalLogicSimulator::getPinState(const Address& address) {
	std::lock_guard lock(mux);
	SimulatorStateIndexVecVariant simulatorIdVariant = getPinSimulatorId_noMux(address);
	if (std::holds_alternative<simulator_state_reference>(simulatorIdVariant)) {
		simulator_state_reference simulatorId = std::get<simulator_state_reference>(simulatorIdVariant);
		return getState(simulatorId);
	} else {
		std::vector<simulator_state_reference> simulatorIds = std::get<std::vector<simulator_state_reference>>(simulatorIdVariant);
		return getStates(simulatorIds);
	}
}

void EvalLogicSimulator::waitForSprintComplete() {
	logicSimulator.waitForSprintComplete();
}

void EvalLogicSimulator::tickStep(unsigned int nTicks) {
	setPause(true);
	logicSimulator.addSprint(nTicks);
	waitForSprintComplete();
}

bool EvalLogicSimulator::stepBack() {
	setPause(true);
	return logicSimulator.stepBack();
}

void EvalLogicSimulator::stepForward() {
	setPause(true);
	bool success = logicSimulator.stepForward();
	if (!success) {
		tickStep();
	}
}

bool EvalLogicSimulator::skipBack() {
	setPause(true);
	return logicSimulator.skipBack();
}

bool EvalLogicSimulator::skipForward() {
	setPause(true);
	return logicSimulator.skipForward();
}

namespace {
	// Tunable tolerance for floating-point comparisons
	inline constexpr double EPS = 1e-12;

	// Decompose x > 0 as x = m * 10^k with m in [1, 10)
	inline void decompose(double x, double& m, int& k) {
		// Start with a log10-based estimate for speed
		double lg = std::log10(x);
		// Add a tiny epsilon to stabilize near powers of 10
		k = static_cast<int>(std::floor(lg + EPS));
		double p = std::pow(10.0, k);
		m = x / p;

		// Correct any edge drift so m in [1, 10)
		while (m < 1.0) { m *= 10.0; --k; }
		while (m >= 10.0) { m /= 10.0; ++k; }
	}

	// True if m is (within EPS) an integer in {1,2,...,9}
	inline bool is_seq_mantissa(double m) {
		double r = std::round(m);
		return std::abs(m - r) <= EPS && r >= 1.0 && r <= 9.0;
	}

	// Smallest sequence value strictly greater than x
	inline double next_in_sequence(double x) {
		if (!(x > 0.0)) return std::numeric_limits<double>::quiet_NaN();

		double m; int k;
		decompose(x, m, k);

		if (is_seq_mantissa(m)) {
			int mi = static_cast<int>(std::llround(m));
			if (mi < 9) {
				return (mi + 1) * std::pow(10.0, k);
			} else {
				// mi == 9 -> roll to 1 * 10^(k+1)
				return std::pow(10.0, k + 1);
			}
		} else {
			// Ceil within the decade
			int cm = static_cast<int>(std::ceil(m - EPS)); // bias to avoid ceil(1.000...+tiny)=2
			if (cm <= 9) {
				return cm * std::pow(10.0, k);
			} else {
				// Hit 10 -> roll to next decade
				return std::pow(10.0, k + 1);
			}
		}
	}

	// Largest sequence value strictly less than x
	inline double prev_in_sequence(double x) {
		if (!(x > 0.0)) return std::numeric_limits<double>::quiet_NaN();

		double m; int k;
		decompose(x, m, k);

		if (is_seq_mantissa(m)) {
			int mi = static_cast<int>(std::llround(m));
			if (mi > 1) {
				return (mi - 1) * std::pow(10.0, k);
			} else {
				// mi == 1 -> roll back to 9 * 10^(k-1)
				return 9.0 * std::pow(10.0, k - 1);
			}
		} else {
			// Floor within the decade
			int fm = static_cast<int>(std::floor(m + EPS)); // bias to avoid floor(0.999...-tiny)=0
			if (fm >= 1) {
				return fm * std::pow(10.0, k);
			} else {
				// Fell below 1 -> use previous decade's 9
				return 9.0 * std::pow(10.0, k - 1);
			}
		}
	}
}

void EvalLogicSimulator::increaseTickrateSeq() {
	double currentTickrate = getTickrate();
	double newTickrate = next_in_sequence(currentTickrate);
	setTickrate(newTickrate);
}

void EvalLogicSimulator::decreaseTickrateSeq() {
	double currentTickrate = getTickrate();
	double newTickrate = std::max(MIN_TICKRATE_DECREASABLE, prev_in_sequence(currentTickrate));
	setTickrate(newTickrate);
}

std::optional<simulator_state_reference> EvalLogicSimulator::getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const {
	std::lock_guard lock(mux);
	return getSimulatorStateIndex_noMux(evalConnectionPoint);
}

SimulatorStateIndexVecVariant EvalLogicSimulator::getVirtualConnectionSimulatorId(const Address& address, virtual_connection_id_t virtualConnectionId) const {
	std::lock_guard lock(mux);
	if (virtualConnectionId != 0) return simulator_state_reference(3);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPoints)) {
		EvalConnectionPoint evalConnectionPoint = std::get<EvalConnectionPoint>(evalConnectionPoints);
		return getSimulatorStateIndex_noMux(evalConnectionPoint).value_or(simulator_state_reference(3));
	}
	std::vector<simulator_state_reference> outputSimulatorIds;
	for (EvalConnectionPoint evalConnectionPoint : std::get<std::vector<EvalConnectionPoint>>(evalConnectionPoints)) {
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndex_noMux(evalConnectionPoint).value_or(simulator_state_reference(3));
		outputSimulatorIds.push_back(simulatorStateIndex);
	}
	return outputSimulatorIds;
	}

SimulatorStateIndexVecVariant EvalLogicSimulator::getPinSimulatorId(const Address& address) const {
	std::lock_guard lock(mux);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPoints)) {
		EvalConnectionPoint evalConnectionPoint = std::get<EvalConnectionPoint>(evalConnectionPoints);
		if (evalConnectionPoint.isNull()) {
			logError("Failed to get bottom connection point.", "EvalLogicSimulator::getPinSimulatorId");
			return simulator_state_reference(0);
		}
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndexConsideringOutput_noMux(evalConnectionPoint).value_or(simulator_state_reference(0));
		return simulatorStateIndex;
	}
	std::vector<simulator_state_reference> outputSimulatorIds;
	for (EvalConnectionPoint evalConnectionPoint : std::get<std::vector<EvalConnectionPoint>>(evalConnectionPoints)) {
		if (evalConnectionPoint.isNull()) {
			logError("Failed to get bottom connection point.", "EvalLogicSimulator::getPinSimulatorId");
			outputSimulatorIds.push_back(simulator_state_reference(3));
			continue;
		}
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndexConsideringOutput_noMux(evalConnectionPoint).value_or(simulator_state_reference(3));
		outputSimulatorIds.push_back(simulatorStateIndex);
	}
	return outputSimulatorIds;
}

std::optional<simulator_state_reference> EvalLogicSimulator::getSimulatorStateIndexConsideringOutput_noMux(EvalConnectionPoint evalConnectionPoint) const {
	const EvalLayerState& evalLayerState = evaluatorInternal.getLayerRunner().getOutputLayer();
	const EvalGate* evalGate = evalLayerState.getGate(evalConnectionPoint.gateId);
	auto connectionsIter = evalGate->connections.find(evalConnectionPoint.connectionEndId);
	eval_gate_id evalGateIdToReadState = evalConnectionPoint.gateId;
	connection_end_id_t connectionEndIdToReadState = evalConnectionPoint.connectionEndId;
	if (connectionsIter != evalGate->connections.end() && connectionsIter->second.size() == 1) {
		const EvalGate* otherSimulatorGate = evalLayerState.getGate(connectionsIter->second.begin()->gateId);
		if (
			otherSimulatorGate->type == getEvalGateType(BlockType::JUNCTION) ||
			otherSimulatorGate->type == getEvalGateType(BlockType::JUNCTION_L) ||
			otherSimulatorGate->type == getEvalGateType(BlockType::JUNCTION_H) ||
			otherSimulatorGate->type == getEvalGateType(BlockType::JUNCTION_X)
		) {
			evalGateIdToReadState = otherSimulatorGate->gateId;
			connectionEndIdToReadState = connection_end_id_t(0);
		}
	}
	return getSimulatorStateIndex_noMux(EvalConnectionPoint(evalGateIdToReadState, connectionEndIdToReadState));
}

std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> EvalLogicSimulator::getPinAndNotPinSimulatorId(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const {
	std::lock_guard lock(mux);
	return getPinAndNotPinSimulatorId_noMux(connectionPoints);
}

std::vector<SimulatorStateIndexVecVariant> EvalLogicSimulator::getVirtualConnectionSimulatorIds(const Address& addressOrigin, const std::vector<std::pair<Position, virtual_connection_id_t>>& virtualConnections) const {
	std::lock_guard lock(mux);
	std::vector<SimulatorStateIndexVecVariant> output;
	for (std::pair<Position, virtual_connection_id_t> virtualConnection : virtualConnections) {
		Address address = addressOrigin;
		address.appendPosition(virtualConnection.first);
		output.push_back(getVirtualConnectionSimulatorId_noMux(address, virtualConnection.second));
	}
	return output;
}

std::vector<SimulatorStateIndexVecVariant> EvalLogicSimulator::getPinSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const {
	std::lock_guard lock(mux);
	std::vector<SimulatorStateIndexVecVariant> output;
	for (Position position : positions) {
		Address address = addressOrigin;
		address.appendPosition(position);
		output.push_back(getPinSimulatorId_noMux(address));
	}
	return output;
}

// unsigned long long addedGateCount = 0;
// unsigned long long removedGateCount = 0;
// unsigned long long addedConnectionCount = 0;
// unsigned long long removedConnectionCount = 0;

// void printCounts() {
// 	std::cout << "addedGateCount:         " << addedGateCount << "\n";
// 	std::cout << "removedGateCount:       " << removedGateCount << "\n";
// 	std::cout << "addedConnectionCount:   " << addedConnectionCount << "\n";
// 	std::cout << "removedConnectionCount: " << removedConnectionCount << std::endl;
// }

void EvalLogicSimulator::processEdits() {
	std::lock_guard lock(mux);
	const EvalLayerState& evalLayerState = evaluatorInternal.getLayerRunner().getOutputLayer();
	// addedGateCount += evalLayerState.getAddedGates().size();
	// removedGateCount += evalLayerState.getRemovedGates().size();
	// addedConnectionCount += evalLayerState.getAddedConnections().size();
	// removedConnectionCount += evalLayerState.getRemovedConnections().size();
	// printCounts();
	{
		for (auto iter : evalLayerState.getRemovedConnections()) {
			logicSimulator.removeConnection(iter.first, iter.second);
		}
		for (auto iter : evalLayerState.getRemovedGates()) {
			logicSimulator.removeGate(iter.first);
		}
		for (auto iter : evalLayerState.getAddedGates()) {
			logicSimulator.addGate(iter.first, getBlockType(iter.second));
		}
		for (auto iter : evalLayerState.getAddedConnections()) {
			logicSimulator.addConnection(iter.first, iter.second);
		}
		logicSimulator.endEdit();
	}

	if (simulatorMappingUpdateListeners.empty()) return;

	std::unordered_set<eval_gate_id> idsToUpdate = evalLayerState.getGateIdRemappingsUpdateds();
	// for (simulator_state_reference simId : dirtySimulatorIds) { // I dont think I need this yet
	// 	idsToUpdate.insert()
	// }
	for (EvalConnectionPoint connectionPoint : evalLayerState.getConnectionPointRemappingsUpdated()) {
		idsToUpdate.insert(connectionPoint.gateId);
	}

	std::vector<EvalConnectionPoint> connectionPointsToUpdate;
	for (eval_gate_id gateId : idsToUpdate) {
		const EvalGate* evalGate = evalLayerState.getGate(gateId);
		// if (!evalGate) continue; // maybe tmp
		assert(evalGate);
		const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(getBlockType(evalGate->type));
		assert(blockData);
		for (auto pair : blockData->getConnectionsSafe()) connectionPointsToUpdate.emplace_back(evalGate->gateId, pair.first);
	}

	std::vector<EvalConnectionPoint> aboveBusLayerConnectionPointsToUpdate = evaluatorInternal.getLayerRunner().getReversedMappedConnectionPointsAboveBusLayer(connectionPointsToUpdate);

	std::vector<std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>>> aboveBusLayerConnectionPointsToUpdateMappedDown = (
		evaluatorInternal.getLayerRunner().getMappedConnectionPointsFromBusLayer(aboveBusLayerConnectionPointsToUpdate)
	);
	assert(aboveBusLayerConnectionPointsToUpdateMappedDown.size() == aboveBusLayerConnectionPointsToUpdate.size());


	const Circuit* circuit = circuitManager.getCircuit(circuitId).get();
	for (auto iter : simulatorMappingUpdateListeners) {
		circuit_id_t otherCircuitId = circuit->getCircuitId(iter.second.address);
		const Circuit* otherCircuit = circuitManager.getCircuit(otherCircuitId).get();
		if (!otherCircuit) {
			logError("Could not find circuit with id {}.", "EvalLogicSimulator::processEdits", otherCircuitId);
			continue;
		}
		const EvaluatorInternal& otherEvaluatorInternal = otherCircuit->getEvaluator().getEvaluatorInternal();
		VecVecEvalConnectionPoint topConnectionPoints = evaluatorInternal.mapFromBottomConnectionPointsToTopConnectionPointsForOtherEvals(
			aboveBusLayerConnectionPointsToUpdate,
			iter.second.address
		);
		assert(topConnectionPoints.size() == aboveBusLayerConnectionPointsToUpdateMappedDown.size());
		std::vector<SimulatorMappingUpdate> simulatorMappingUpdates;
		for (unsigned int i = 0; i < topConnectionPoints.size(); i++) {
			if (topConnectionPoints[i].empty()) continue;;
			std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> simulatorIndexes = getPinAndNotPinSimulatorId_noMux(aboveBusLayerConnectionPointsToUpdateMappedDown[i]);
			for (EvalConnectionPoint connectionPoint : topConnectionPoints[i]) {
				std::optional<std::pair<Position, Position>> addressesPair = otherEvaluatorInternal.mapFromTopConnectionPointToPointAndBlockPosition(connectionPoint);
				assert(addressesPair.has_value());
				simulatorMappingUpdates.emplace_back(addressesPair->first, simulatorIndexes.first);
				simulatorMappingUpdates.emplace_back(addressesPair->second, 0, simulatorIndexes.second);
			}
		}
		iter.second.callback(simulatorMappingUpdates);
	}
}

void EvalLogicSimulator::connectListener(void* object, const Address& address, SimulatorMappingUpdateListenerFunction func) const {
	std::lock_guard lock(mux);
	simulatorMappingUpdateListeners.try_emplace(object, address, func);
}


void EvalLogicSimulator::disconnectListener(void* object) const {
	std::lock_guard lock(mux);
	auto iter = simulatorMappingUpdateListeners.find(object);
	if (iter != simulatorMappingUpdateListeners.end()) {
		simulatorMappingUpdateListeners.erase(iter);
	}
}


std::optional<simulator_state_reference> EvalLogicSimulator::getSimulatorStateIndex_noMux(EvalConnectionPoint evalConnectionPoint) const {
	return logicSimulator.getSimulatorStateIndex(evalConnectionPoint);
}

SimulatorStateIndexVecVariant EvalLogicSimulator::getVirtualConnectionSimulatorId_noMux(const Address& address, virtual_connection_id_t virtualConnectionId) const {
	if (virtualConnectionId != 0) return simulator_state_reference(3);
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPoints)) {
		return getSimulatorStateIndex_noMux(std::get<EvalConnectionPoint>(evalConnectionPoints)).value_or(simulator_state_reference(3));
	}
	std::vector<simulator_state_reference> outputSimulatorIds;
	for (EvalConnectionPoint evalConnectionPoint : std::get<std::vector<EvalConnectionPoint>>(evalConnectionPoints)) {
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndex_noMux(evalConnectionPoint).value_or(simulator_state_reference(3));
		outputSimulatorIds.push_back(simulatorStateIndex);
	}
	return outputSimulatorIds;
	}

SimulatorStateIndexVecVariant EvalLogicSimulator::getPinSimulatorId_noMux(const Address& address) const {
	std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> evalConnectionPoints = evaluatorInternal.mapFromAddressToBottomConnectionPoints(address);
	if (std::holds_alternative<EvalConnectionPoint>(evalConnectionPoints)) {
		EvalConnectionPoint evalConnectionPoint = std::get<EvalConnectionPoint>(evalConnectionPoints);
		if (evalConnectionPoint.isNull()) {
			logError("Failed to get bottom connection point.", "EvalLogicSimulator::getPinSimulatorId");
			return simulator_state_reference(0);
		}
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndexConsideringOutput_noMux(evalConnectionPoint).value_or(simulator_state_reference(0));
		return simulatorStateIndex;
	}
	std::vector<simulator_state_reference> outputSimulatorIds;
	for (EvalConnectionPoint evalConnectionPoint : std::get<std::vector<EvalConnectionPoint>>(evalConnectionPoints)) {
		if (evalConnectionPoint.isNull()) {
			logError("Failed to get bottom connection point.", "EvalLogicSimulator::getPinSimulatorId");
			outputSimulatorIds.push_back(simulator_state_reference(3));
			continue;
		}
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndexConsideringOutput_noMux(evalConnectionPoint).value_or(simulator_state_reference(3));
		outputSimulatorIds.push_back(simulatorStateIndex);
	}
	return outputSimulatorIds;
}

std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> EvalLogicSimulator::getPinAndNotPinSimulatorId_noMux(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const {
	if (std::holds_alternative<EvalConnectionPoint>(connectionPoints)) {
		return getPinAndNotPinSimulatorId_noMux(std::get<EvalConnectionPoint>(connectionPoints));
	}
	std::vector<simulator_state_reference> outputNonPinIds;
	std::vector<simulator_state_reference> outputPinIds;
	for (EvalConnectionPoint evalConnectionPoint : std::get<std::vector<EvalConnectionPoint>>(connectionPoints)) {
		std::pair<simulator_state_reference, simulator_state_reference> pinAndNonPinIds = getPinAndNotPinSimulatorId_noMux(evalConnectionPoint);
		outputPinIds.push_back(pinAndNonPinIds.first);
		outputNonPinIds.push_back(pinAndNonPinIds.second);
	}
	return {outputPinIds, outputNonPinIds};
}

std::pair<simulator_state_reference, simulator_state_reference> EvalLogicSimulator::getPinAndNotPinSimulatorId_noMux(EvalConnectionPoint connectionPoint) const {
	std::optional<simulator_state_reference> nonPinStateIndexOpt = getSimulatorStateIndex_noMux(connectionPoint);
	std::optional<simulator_state_reference> pinStateIndexOpt = getSimulatorStateIndexConsideringOutput_noMux(connectionPoint);
	if (!nonPinStateIndexOpt.has_value() || !pinStateIndexOpt.has_value()) {
		logError("Failed to get sim state indices.", "EvalLogicSimulator::getPinAndNotPinSimulatorId");
		return {simulator_state_reference(3), simulator_state_reference(3)};
	}
	return {pinStateIndexOpt.value(), nonPinStateIndexOpt.value()};
}
