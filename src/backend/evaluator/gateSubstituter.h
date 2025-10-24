#ifndef gateSubstituter_h
#define gateSubstituter_h

#include "logicSimulator.h"
#include "evalConnection.h"
#include "evalConfig.h"
#include "evalDefs.h"
#include "replacer.h"

struct TrackedGate {
	middle_id_t id;
	BlockType currentState;
	BlockType falseState;
	BlockType trueState;
	std::vector<EvalConnection> inputs;
	std::vector<EvalConnection> outputs;
	unsigned int numInputsForTrue;

	BlockType evaluate() {
		if (inputs.size() >= numInputsForTrue) {
			return trueState;
		} else {
			return falseState;
		}
	}

	void addInput(EvalConnection connection) {
		inputs.push_back(connection);
	}

	void addOutput(EvalConnection connection) {
		outputs.push_back(connection);
	}

	void removeInput(EvalConnection connection) {
		auto it = std::find_if(inputs.begin(), inputs.end(),
			[&](const EvalConnection& conn) { return conn.source == connection.source && conn.destination == connection.destination; });
		if (it != inputs.end()) {
			inputs.erase(it);
		}
	}

	void removeOutput(EvalConnection connection) {
		auto it = std::find_if(outputs.begin(), outputs.end(),
			[&](const EvalConnection& conn) { return conn.source == connection.source && conn.destination == connection.destination; });
		if (it != outputs.end()) {
			outputs.erase(it);
		}
	}

	bool removeReferencesToId(const middle_id_t id) {
		size_t initialSize = inputs.size() + outputs.size();
		inputs.erase(std::remove_if(inputs.begin(), inputs.end(),
			[&](const EvalConnection& conn) { return conn.source.gateId == id || conn.destination.gateId == id; }), inputs.end());
		outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
			[&](const EvalConnection& conn) { return conn.source.gateId == id || conn.destination.gateId == id; }), outputs.end());
		return (initialSize != inputs.size() + outputs.size());
	}
};

struct GateWithLinkedIO {
	middle_id_t id;
	std::vector<middle_id_t> idsCreated;
	std::unordered_map<connection_port_id_t, EvalConnectionPoint> linkedIO;
};

class GateSubstituter {
public:
	GateSubstituter(
		EvalConfig& evalConfig,
		IdProvider<middle_id_t>& middleIdProvider,
		std::vector<simulator_id_t>& dirtySimulatorIds,
		std::vector<middle_id_t>& dirtyMiddleIds,
		BlockDataManager& blockDataManager) :
		replacer(evalConfig, middleIdProvider, dirtySimulatorIds, dirtyMiddleIds, blockDataManager),
		middleIdProvider(middleIdProvider),
		blockDataManager(blockDataManager) {}

	void addGate(SimPauseGuard& pauseGuard, const BlockType blockType, const middle_id_t gateId) {
		if (tryAddGateWithLinkedIO(pauseGuard, blockType, gateId)) return;
		if (blockType == BlockType::BUTTON || blockType == BlockType::SWITCH || blockType == BlockType::TICK_BUTTON) {
			addTrackedGate({ gateId, blockType, blockType, BlockType::JUNCTION, {}, {}, 1 });
			replacer.addGate(pauseGuard, blockType, gateId);
		} else if (blockType == BlockType::LIGHT) {
			replacer.addGate(pauseGuard, BlockType::JUNCTION, gateId);
		} else {
			replacer.addGate(pauseGuard, blockType, gateId);
		}
	}
	void removeGate(SimPauseGuard& pauseGuard, const middle_id_t gateId) {
		if (gatesWithLinkedIO.contains(gateId)) {
			deleteGateWithLinkedIO(pauseGuard, gateId);
		}
		replacer.removeGate(pauseGuard, gateId);
		deleteTrackedGate(gateId);
		for (auto& trackedGate : trackedGates) {
			bool success = trackedGate.second.removeReferencesToId(gateId);
			if (success) {
				TrackedGate& trackedGateRef = trackedGate.second;
				BlockType newState = trackedGateRef.evaluate();
				if (newState != trackedGateRef.currentState) {
					trackedGateRef.currentState = newState;
					replacer.removeGate(pauseGuard, trackedGateRef.id);
					replacer.addGate(pauseGuard, newState, trackedGateRef.id);
					for (const auto& input : trackedGateRef.inputs) {
						replacer.makeConnection(pauseGuard, input);
					}
					for (const auto& output : trackedGateRef.outputs) {
						if (output.source.gateId == output.destination.gateId) {
							continue;
						}
						replacer.makeConnection(pauseGuard, output);
					}
				}
			}
		}
	}
	inline SimPauseGuard beginEdit() {
		return replacer.beginEdit();
	}
	inline void endEdit(SimPauseGuard& pauseGuard) {
		replacer.endEdit(pauseGuard);
	}

	inline logic_state_t getState(EvalConnectionPoint point) const {
		return replacer.getState(point);
	}
	inline std::vector<logic_state_t> getStates(const std::vector<EvalConnectionPoint>& points) const {
		return replacer.getStates(points);
	}
	inline std::vector<logic_state_t> getPinStates(const std::vector<EvalConnectionPoint>& points) const {
		return replacer.getPinStates(points);
	}
	inline std::vector<logic_state_t> getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
		return replacer.getStatesFromSimulatorIds(simulatorIds);
	}
	inline std::vector<simulator_id_t> getBlockSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		return replacer.getBlockSimulatorIds(points);
	}
	inline std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> getPinSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		return replacer.getPinSimulatorIds(points);
	}
	inline void setState(EvalConnectionPoint point, logic_state_t state) {
		replacer.setState(point, state);
	}
	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		middle_id_t sourceGateId = connection.source.gateId;
		middle_id_t destinationGateId = connection.destination.gateId;
		redirectConnectionToLinked(connection);
		if (trackedGates.contains(sourceGateId)) {
			TrackedGate& trackedGate = trackedGates.at(sourceGateId);
			trackedGate.addOutput(connection);
		}
		if (trackedGates.contains(destinationGateId)) {
			TrackedGate& trackedGate = trackedGates.at(destinationGateId);
			trackedGate.addInput(connection);
			BlockType newState = trackedGate.evaluate();
			if (newState == trackedGate.currentState) {
				replacer.makeConnection(pauseGuard, connection);
				return;
			}
			trackedGate.currentState = newState;
			replacer.removeGate(pauseGuard, destinationGateId);
			replacer.addGate(pauseGuard, newState, destinationGateId);
			for (const auto& input : trackedGate.inputs) {
				replacer.makeConnection(pauseGuard, input);
			}
			for (const auto& output : trackedGate.outputs) {
				if (output.source.gateId == output.destination.gateId) {
					continue;
				}
				replacer.makeConnection(pauseGuard, output);
			}
			return;
		}
		replacer.makeConnection(pauseGuard, connection);
	}
	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		middle_id_t sourceGateId = connection.source.gateId;
		middle_id_t destinationGateId = connection.destination.gateId;
		redirectConnectionToLinked(connection);
		replacer.removeConnection(pauseGuard, connection);
		if (trackedGates.contains(sourceGateId)) {
			TrackedGate& trackedGate = trackedGates.at(sourceGateId);
			trackedGate.removeOutput(connection);
		}
		if (trackedGates.contains(destinationGateId)) {
			TrackedGate& trackedGate = trackedGates.at(destinationGateId);
			trackedGate.removeInput(connection);
			BlockType newState = trackedGate.evaluate();
			if (newState != trackedGate.currentState) {
				trackedGate.currentState = newState;
				replacer.removeGate(pauseGuard, destinationGateId);
				replacer.addGate(pauseGuard, newState, destinationGateId);
				for (const auto& input : trackedGate.inputs) {
					replacer.makeConnection(pauseGuard, input);
				}
				for (const auto& output : trackedGate.outputs) {
					if (output.source.gateId == output.destination.gateId) {
						continue;
					}
					replacer.makeConnection(pauseGuard, output);
				}
			}
		}
	}

	inline double getAverageTickrate() const {
		return replacer.getAverageTickrate();
	}

private:
	Replacer replacer;
	IdProvider<middle_id_t>& middleIdProvider;
	BlockDataManager& blockDataManager;
	std::unordered_map<middle_id_t, TrackedGate> trackedGates;
	std::unordered_map<middle_id_t, GateWithLinkedIO> gatesWithLinkedIO;
	void addTrackedGate(const TrackedGate& gate) {
		trackedGates[gate.id] = gate;
	}
	void deleteTrackedGate(middle_id_t gateId) {
		trackedGates.erase(gateId);
	}
	bool isTrackedGate(middle_id_t gateId) const {
		return trackedGates.contains(gateId);
	}
	bool tryAddGateWithLinkedIO(SimPauseGuard& pauseGuard, BlockType blockType, middle_id_t gateId) {
		if (blockType == BlockType::COLOR_LIGHT) { // this will be expanded later to be dynamic/automatic for all blocks that have non 1-bit inputs
			replacer.addGate(pauseGuard, blockType, gateId);
			middle_id_t busId = middleIdProvider.getNewId();
			replacer.addGate(pauseGuard, blockDataManager.getBusBlock(6), busId);
			GateWithLinkedIO gateWithLinkedIO { gateId, { busId }, { {0, { busId, 6 }} } };
			for (connection_port_id_t portId = 0; portId < 6; ++portId) {
				replacer.makeConnection(pauseGuard, EvalConnection(EvalConnectionPoint(busId, portId), EvalConnectionPoint(gateId, portId)));
			}
			gatesWithLinkedIO[gateId] = std::move(gateWithLinkedIO);
			return true;
		}
		auto configIter = blockTypesWithlinkedIO.find(blockType);
		if (configIter == blockTypesWithlinkedIO.end()) {
			return false;
		}
		replacer.addGate(pauseGuard, blockType, gateId);
		GateWithLinkedIO gateWithLinkedIO { gateId, {}, {} };
		for (connection_port_id_t portId : configIter->second) {
			middle_id_t junctionId = middleIdProvider.getNewId();
			replacer.addGate(pauseGuard, BlockType::JUNCTION, junctionId);
			replacer.makeConnection(pauseGuard, EvalConnection(EvalConnectionPoint(junctionId, 0), EvalConnectionPoint(gateId, portId)));
			gateWithLinkedIO.idsCreated.push_back(junctionId);
			gateWithLinkedIO.linkedIO[portId] = EvalConnectionPoint(junctionId, 0);
		}
		gatesWithLinkedIO[gateId] = std::move(gateWithLinkedIO);
		return true;
	}
	void deleteGateWithLinkedIO(SimPauseGuard& pauseGuard, middle_id_t gateId) {
		if (!gatesWithLinkedIO.contains(gateId)) {
			return;
		}
		GateWithLinkedIO& gate = gatesWithLinkedIO.at(gateId);
		for (middle_id_t middleId : gate.idsCreated) {
			replacer.removeGate(pauseGuard, middleId);
			middleIdProvider.releaseId(middleId);
		}
		gatesWithLinkedIO.erase(gateId);
	}
	void redirectConnectionToLinked(EvalConnection& connection) {
		middle_id_t sourceGateId = connection.source.gateId;
		middle_id_t destinationGateId = connection.destination.gateId;
		if (gatesWithLinkedIO.contains(destinationGateId)) {
			GateWithLinkedIO& gate = gatesWithLinkedIO.at(destinationGateId);
			if (gate.linkedIO.contains(connection.destination.portId)) {
				connection.destination = gate.linkedIO.at(connection.destination.portId);
			}
		}
		if (gatesWithLinkedIO.contains(sourceGateId)) {
			GateWithLinkedIO& gate = gatesWithLinkedIO.at(sourceGateId);
			if (gate.linkedIO.contains(connection.source.portId)) {
				connection.source = gate.linkedIO.at(connection.source.portId);
			}
		}
	}

	std::map<BlockType, std::vector<connection_port_id_t>> blockTypesWithlinkedIO = {
		{BlockType::TRISTATE_BUFFER, {0, 1}},
		{BlockType::NOT, {0}},
		{BlockType::BUFFER, {0}}
	};
};

#endif /* gateSubstituter_h */
