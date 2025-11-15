#ifndef simulatorOptimizer_h
#define simulatorOptimizer_h

#include "backend/evaluator/simulator/logicSimulator.h"
#include "backend/evaluator/util/evalConnection.h"
#include "backend/evaluator/util/evalConfig.h"

class SimulatorOptimizer {
public:
	SimulatorOptimizer(
		EvalConfig& evalConfig,
		IdProvider<middle_id_t>& middleIdProvider,
		std::vector<simulator_id_t>& dirtySimulatorIds) :
		simulator(evalConfig, dirtySimulatorIds),
		evalConfig(evalConfig),
		middleIdProvider(middleIdProvider) {
		// inputConnections.resize(1000);
		// outputConnections.resize(1000);
		// middleToSimulatorIds.resize(1000);
	}

	void addGate(SimPauseGuard& pauseGuard, const BlockType blockType, const middle_id_t gateId);
	void removeGate(SimPauseGuard& pauseGuard, const middle_id_t gateId);
	SimPauseGuard beginEdit() {
		return SimPauseGuard(simulator);
	}
	void endEdit(SimPauseGuard& pauseGuard) {
		simulator.endEdit();
	};

	std::optional<simulator_id_t> getSimIdFromMiddleId(middle_id_t middleId) const {
		if (middleId < middleToSimulatorIds.size()) {
			return middleToSimulatorIds[middleId];
		}
		return std::nullopt;
	}
	std::optional<simulator_id_t> getSimIdFromConnectionPoint(const EvalConnectionPoint& point) const {
		std::optional<simulator_id_t> gateId = getSimIdFromMiddleId(point.gateId);
		if (!gateId.has_value()) {
			// logError("Gate ID not found for connection point", "SimulatorOptimizer::getSimIdFromConnectionPoint");
			return std::nullopt;
		}
		return simulator.getOutputPortId(gateId.value(), point.portId);
	}

	logic_state_t getState(EvalConnectionPoint point) const {
		std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
		if (!simIdOpt.has_value()) {
			logError("Sim ID not found for connection point", "SimulatorOptimizer::getState");
			return logic_state_t::UNDEFINED; // or some other default state
		}
		simulator_id_t simId = simIdOpt.value();
		return simulator.getState(simId);
	}
	std::vector<logic_state_t> getStates(const std::vector<EvalConnectionPoint>& points) const {
		std::vector<simulator_id_t> simIds;
		simIds.reserve(points.size());
		for (const auto& point : points) {
			std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
			simIds.push_back(simIdOpt.value_or(simulator_id_t(0)));
		}
		return simulator.getStates(simIds);
	}
	std::vector<logic_state_t> getPinStates(const std::vector<EvalConnectionPoint>& points) const {
		std::vector<simulator_id_t> simIds;
		simIds.reserve(points.size());
		for (const auto& point : points) {
			// get num outputs
			int numOutputs = getNumOutputs(point.gateId);
			if (numOutputs == 1) {
				std::vector<EvalConnection> outputs = getOutputs(point.gateId);
				EvalConnection output = outputs.at(0);
				BlockType blockType = getBlockType(output.destination.gateId);
				if (isJunctionType(blockType)) {
					// get the simId of the output
					std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(output.destination);
					simIds.push_back(simIdOpt.value_or(simulator_id_t(0)));
					continue;
				}
			}
			std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
			simIds.push_back(simIdOpt.value_or(simulator_id_t(0)));
		}
		return simulator.getStates(simIds);
	}
	inline std::vector<logic_state_t> getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
		return simulator.getStates(simulatorIds);
	}
	simulator_id_t getBlockSimulatorId(EvalConnectionPoint point) const {
		std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
		return simIdOpt.value_or(simulator_id_t(0));
	}
	simulator_id_t getPinSimulatorId(EvalConnectionPoint point) const {
		std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
		if (!simIdOpt.has_value()) {
			return simulator_id_t(0);
		}
		int numOutputs = getNumOutputs(point.gateId);
		if (numOutputs == 1) {
			std::vector<EvalConnection> outputs = getOutputs(point.gateId);
			EvalConnection output = outputs.at(0);
			BlockType blockType = getBlockType(output.destination.gateId);
			if (isJunctionType(blockType)) {
				// get the simId of the output
				std::optional<simulator_id_t> pinSimIdOpt = getSimIdFromConnectionPoint(output.destination);
				return pinSimIdOpt.value_or(simulator_id_t(0));
			}
		}
		return simIdOpt.value();
	}
	std::vector<simulator_id_t> getBlockSimulatorIds(const std::vector<EvalConnectionPoint>& points) const {
		std::vector<simulator_id_t> result;
		result.reserve(points.size());
		for (const auto& point : points) {
			result.push_back(getBlockSimulatorId(point));
		}
		return result;
	}
	std::vector<simulator_id_t> getPinSimulatorIds(const std::vector<EvalConnectionPoint>& points) const {
		std::vector<simulator_id_t> result;
		result.reserve(points.size());
		for (const auto& point : points) {
			result.push_back(getPinSimulatorId(point));
		}
		return result;
	}
	std::vector<simulator_id_t> getBlockSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		std::vector<simulator_id_t> result;
		result.reserve(points.size());
		for (const auto& pointOpt : points) {
			if (!pointOpt.has_value()) {
				result.push_back(simulator_id_t(0));
				continue;
			}
			result.push_back(getBlockSimulatorId(pointOpt.value()));
		}
		return result;
	}
	std::vector<simulator_id_t> getPinSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		std::vector<simulator_id_t> result;
		result.reserve(points.size());
		for (const auto& pointOpt : points) {
			if (!pointOpt.has_value()) {
				result.push_back(simulator_id_t(0));
				continue;
			}
			result.push_back(getPinSimulatorId(pointOpt.value()));
		}
		return result;
	}
	void setState(EvalConnectionPoint point, logic_state_t state) {
		std::optional<simulator_id_t> simIdOpt = getSimIdFromConnectionPoint(point);
		if (!simIdOpt.has_value()) {
			logError("Sim ID not found for connection point", "SimulatorOptimizer::setState");
			return;
		}
		BlockType blockType = getBlockType(point.gateId);
		if (blockType == BlockType::CONSTANT_ON || blockType == BlockType::CONSTANT_OFF ||
			blockType == BlockType::CONSTANT_Z || blockType == BlockType::CONSTANT_X) {
			// cannot set state of constant blocks
			return;
		}
		simulator.setState(simIdOpt.value(), state);
	}
	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection);
	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection);

	std::vector<EvalConnection> getInputs(middle_id_t middleId) const;
	std::vector<EvalConnection> getOutputs(middle_id_t middleId) const;
	int getNumInputs(middle_id_t middleId) const {
		if (middleId < inputConnections.size()) {
			return static_cast<int>(inputConnections[middleId].size());
		}
		return 0;
	}
	int getNumOutputs(middle_id_t middleId) const {
		if (middleId < outputConnections.size()) {
			return static_cast<int>(outputConnections[middleId].size());
		}
		return 0;
	}
	BlockType getBlockType(middle_id_t middleId) const {
		if (middleId < blockTypes.size()) {
			return blockTypes[middleId];
		}
		return BlockType::NONE;
	}

	inline double getAverageTickrate() const {
		return simulator.getAverageTickrate();
	}
	inline bool stepBack() {
		SimPauseGuard pauseGuard(simulator);
		return simulator.stepBack();
	}
	inline bool stepForward() {
		SimPauseGuard pauseGuard(simulator);
		return simulator.stepForward();
	}
	inline bool skipBack() {
		SimPauseGuard pauseGuard(simulator);
		return simulator.skipBack();
	}
	inline bool skipForward() {
		SimPauseGuard pauseGuard(simulator);
		return simulator.skipForward();
	}
	inline bool isViewingReplay() const {
		return simulator.isViewingReplay();
	}
	nlohmann::json dumpState() const;

private:
	LogicSimulator simulator;
	EvalConfig& evalConfig;
	IdProvider<middle_id_t>& middleIdProvider;
	IdVector<simulator_id_t, middle_id_t> simulatorToMiddleIds; // maps simulator_id_t to middle_id_t
	IdVector<middle_id_t, simulator_id_t> middleToSimulatorIds; // maps middle_id_t to simulator_id_t

	IdVector<middle_id_t, std::vector<EvalConnection>> inputConnections;  // inputConnections[middleId] = connections TO this gate
	IdVector<middle_id_t, std::vector<EvalConnection>> outputConnections; // outputConnections[middleId] = connections FROM this gate
	IdVector<middle_id_t, BlockType> blockTypes; // maps middle_id_t to BlockType
	bool isJunctionType(BlockType blockType) const {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_X;
	}

    nlohmann::json dumpEvalConnectionVector(const std::vector<EvalConnection>& connections) const {
        nlohmann::json connArray = nlohmann::json::array();
        for (const EvalConnection& conn : connections) {
            connArray.push_back(conn.dumpState());
        }
        return connArray;
    }
};

#endif /* simulatorOptimizer_h */
