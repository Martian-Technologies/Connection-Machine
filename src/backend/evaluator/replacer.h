#ifndef replacer_h
#define replacer_h

#include "simulatorOptimizer.h"
#include "evalConfig.h"
#include "evalConnection.h"
#include "evalTypedef.h"
#include "idProvider.h"
#include "gateType.h"
#include "logicSimulator.h"
struct ReplacementGate {
	middle_id_t id;
	GateType type;
};

class Replacement {
public:
	Replacement(SimulatorOptimizer* optimizer, IdProvider<middle_id_t>* middleIdProvider, 
	            std::unordered_map<middle_id_t, middle_id_t>* replacedIds) :
		simulatorOptimizer(optimizer), middleIdProvider(middleIdProvider), replacedIds(replacedIds) {}

	void removeGate(SimPauseGuard& pauseGuard, middle_id_t gateId, middle_id_t replacementId) {
		isEmpty = false;
		// track connection removals
		std::vector<EvalConnection> outputs = simulatorOptimizer->getOutputs(gateId);
		std::vector<EvalConnection> inputs = simulatorOptimizer->getInputs(gateId);
		for (const auto& conn : outputs) {
			if (conn.destination.gateId != conn.source.gateId) {
				deletedConnections.push_back(conn);
			}
		}
		for (const auto& conn : inputs) {
			deletedConnections.push_back(conn);
		}
		deletedGates.push_back({ gateId, simulatorOptimizer->getGateType(gateId) });
		idsToTrackInputs.insert(gateId);
		idsToTrackOutputs.insert(gateId);
		replacedIds->insert({ gateId, replacementId });
		simulatorOptimizer->removeGate(pauseGuard, gateId);
	}

	void addGate(SimPauseGuard& pauseGuard, GateType gateType, middle_id_t gateId) {
		isEmpty = false;
		simulatorOptimizer->addGate(pauseGuard, gateType, gateId);
		// we don't need to track, because nothing can happen to this gate
		addedGates.push_back({ gateId, gateType });
	}

	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		isEmpty = false;
		simulatorOptimizer->removeConnection(pauseGuard, connection);
		idsToTrackInputs.insert(connection.destination.gateId);
		idsToTrackOutputs.insert(connection.source.gateId);
		deletedConnections.push_back(connection);
	}

	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		isEmpty = false;
		simulatorOptimizer->makeConnection(pauseGuard, connection);
		idsToTrackInputs.insert(connection.destination.gateId);
		idsToTrackOutputs.insert(connection.source.gateId);
		addedConnections.push_back(connection);
	}

	void revert(SimPauseGuard& pauseGuard) {
		isEmpty = true;
		for (const auto& conn : addedConnections) {
			simulatorOptimizer->removeConnection(pauseGuard, conn);
		}
		for (const auto& conn : addedGates) {
			simulatorOptimizer->removeGate(pauseGuard, conn.id);
		}
		for (const auto& gate : deletedGates) {
			simulatorOptimizer->addGate(pauseGuard, gate.type, gate.id);
			replacedIds->erase(gate.id);
		}
		for (const auto& conn : deletedConnections) {
			simulatorOptimizer->makeConnection(pauseGuard, conn);
		}
		for (const auto& id : reservedIds) {
			middleIdProvider->releaseId(id);
		}
		addedConnections.clear();
		addedGates.clear();
		deletedConnections.clear();
		deletedGates.clear();
		reservedIds.clear();
		idsToTrackInputs.clear();
		idsToTrackOutputs.clear();
	}

	void pingOutput(SimPauseGuard& pauseGuard, middle_id_t id) {
		if (idsToTrackOutputs.contains(id)) {
			revert(pauseGuard);
		}
	}

	void pingInput(SimPauseGuard& pauseGuard, middle_id_t id) {
		if (idsToTrackInputs.contains(id)) {
			revert(pauseGuard);
		}
	}

	bool getIsEmpty() const {
		return isEmpty;
	}

	middle_id_t getNewId() {
		middle_id_t newId = middleIdProvider->getNewId();
		reservedIds.push_back(newId);
		return newId;
	}

private:
	SimulatorOptimizer* simulatorOptimizer;
	IdProvider<middle_id_t>* middleIdProvider;
	std::unordered_map<middle_id_t, middle_id_t>* replacedIds;
	std::vector<ReplacementGate> addedGates;
	std::vector<ReplacementGate> deletedGates;
	std::vector<EvalConnection> addedConnections;
	std::vector<EvalConnection> deletedConnections;
	std::vector<middle_id_t> reservedIds;
	std::set<middle_id_t> idsToTrackOutputs;
	std::set<middle_id_t> idsToTrackInputs;
	bool isEmpty { true };
};

class Replacer {
public:
	Replacer(EvalConfig& evalConfig, IdProvider<middle_id_t>& middleIdProvider) :
		simulatorOptimizer(evalConfig, middleIdProvider), evalConfig(evalConfig), middleIdProvider(middleIdProvider) {}

	void addGate(SimPauseGuard& pauseGuard, const GateType gateType, const middle_id_t gateId) {
		simulatorOptimizer.addGate(pauseGuard, gateType, gateId);
	}

	void removeGate(SimPauseGuard& pauseGuard, const middle_id_t gateId) {
		pingOutputs(pauseGuard, gateId);
		pingInputs(pauseGuard, gateId);
		simulatorOptimizer.removeGate(pauseGuard, gateId);
	}

	SimPauseGuard beginEdit() {
		return simulatorOptimizer.beginEdit();
	}

	void endEdit(SimPauseGuard& pauseGuard) {
		cleanReplacements();
		mergeJunctions(pauseGuard);

		simulatorOptimizer.endEdit(pauseGuard);
	}

	std::optional<simulator_id_t> getSimIdFromMiddleId(middle_id_t middleId) const {
		return simulatorOptimizer.getSimIdFromMiddleId(middleId);
	}

	std::optional<simulator_id_t> getSimIdFromConnectionPoint(const EvalConnectionPoint& point) const {
		return simulatorOptimizer.getSimIdFromConnectionPoint(point);
	}

	logic_state_t getState(EvalConnectionPoint point) const {
		return simulatorOptimizer.getState(getReplacementConnectionPoint(point));
	}

	std::vector<logic_state_t> getStates(const std::vector<EvalConnectionPoint>& points) const {
		return simulatorOptimizer.getStates(getReplacementConnectionPoints(points));
	}

	void setState(EvalConnectionPoint id, logic_state_t state) {
		simulatorOptimizer.setState(getReplacementConnectionPoint(id), state);
	}

	void setStates(const std::vector<EvalConnectionPoint>& points, const std::vector<logic_state_t>& states) {
		simulatorOptimizer.setStates(getReplacementConnectionPoints(points), states);
	}

	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingOutputs(pauseGuard, connection.source.gateId);
		pingInputs(pauseGuard, connection.destination.gateId);
		simulatorOptimizer.makeConnection(pauseGuard, connection);
	}

	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingOutputs(pauseGuard, connection.source.gateId);
		pingInputs(pauseGuard, connection.destination.gateId);
		simulatorOptimizer.removeConnection(pauseGuard, connection);
	}

	unsigned int getAverageTickrate() const {
		return simulatorOptimizer.getAverageTickrate();
	}

private:
	SimulatorOptimizer simulatorOptimizer;
	EvalConfig& evalConfig;
	IdProvider<middle_id_t>& middleIdProvider;
	std::vector<Replacement> replacements;
	std::unordered_map<middle_id_t, middle_id_t> replacedIds;
	Replacement& makeReplacement() {
		replacements.push_back(Replacement(&simulatorOptimizer, &middleIdProvider, &replacedIds));
		return replacements.back();
	}
	void cleanReplacements() {
		for (auto it = replacements.begin(); it != replacements.end();) {
			if (it->getIsEmpty()) {
				it = replacements.erase(it);
			} else {
				++it;
			}
		}
	}
	void pingOutputs(SimPauseGuard& pauseGuard, middle_id_t id) {
		for (auto& replacement : replacements) {
			replacement.pingOutput(pauseGuard, id);
		}
	}
	void pingInputs(SimPauseGuard& pauseGuard, middle_id_t id) {
		for (auto& replacement : replacements) {
			replacement.pingInput(pauseGuard, id);
		}
	}
	middle_id_t getReplacementId(middle_id_t id) const {
		if (replacedIds.contains(id)) {
			return replacedIds.at(id);
		}
		return id;
	}
	EvalConnectionPoint getReplacementConnectionPoint(EvalConnectionPoint point) const {
		return EvalConnectionPoint(getReplacementId(point.gateId), point.portId);
	}
	std::vector<middle_id_t> getReplacementIds(const std::vector<middle_id_t>& ids) {
		std::vector<middle_id_t> result;
		result.reserve(ids.size());
		for (const auto& id : ids) {
			result.push_back(getReplacementId(id));
		}
		return result;
	}
	std::vector<EvalConnectionPoint> getReplacementConnectionPoints(const std::vector<EvalConnectionPoint>& points) const {
		std::vector<EvalConnectionPoint> result;
		result.reserve(points.size());
		for (const auto& point : points) {
			result.push_back(getReplacementConnectionPoint(point));
		}
		return result;
	}

	struct JunctionFloodFillResult {
		std::vector<EvalConnectionPoint> outputsGoingIntoJunctions;
		std::vector<EvalConnection> inputsPullingFromJunctions;
		std::vector<middle_id_t> junctionIds;
		std::vector<EvalConnection> connectionsToReroute;
	};

	void mergeJunctions(SimPauseGuard& pauseGuard) {
		std::vector<middle_id_t> allMiddleIds = middleIdProvider.getUsedIds();
		for (const middle_id_t id : allMiddleIds) {
			// if this id is in the replacedIds map, it means we can skip it / it's already been scanned and replaced
			if (replacedIds.contains(id)) {
				continue;
			}
			// check if we're a junction
			GateType gateType = simulatorOptimizer.getGateType(id);
			if (gateType != GateType::JUNCTION) {
				continue;
			}
			JunctionFloodFillResult floodFillResult = junctionFloodFill(id);
			// print the result for debug
			if (floodFillResult.junctionIds.size() < 2 && floodFillResult.connectionsToReroute.empty()) {
				continue;
			}
			// logInfo("Junction flood fill result for ID {}: {} inputs, {} outputs, {} junctions", "Replacer::mergeJunctions",
			// 	id, floodFillResult.inputsPullingFromJunctions.size(), floodFillResult.outputsGoingIntoJunctions.size(), floodFillResult.junctionIds.size());
			Replacement& replacement = makeReplacement();
			middle_id_t newJunctionId = replacement.getNewId();
			replacement.addGate(pauseGuard, GateType::JUNCTION, newJunctionId);
			for (const auto& junctionId : floodFillResult.junctionIds) {
				replacement.removeGate(pauseGuard, junctionId, newJunctionId);
			}
			for (const auto& input : floodFillResult.inputsPullingFromJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(EvalConnectionPoint(newJunctionId, 0), input.destination));
			}
			for (const auto& output : floodFillResult.outputsGoingIntoJunctions) {
				replacement.makeConnection(pauseGuard, EvalConnection(output, EvalConnectionPoint(newJunctionId, 0)));
			}
			for (const auto& conn : floodFillResult.connectionsToReroute) {
				EvalConnection newConnection = EvalConnection(EvalConnectionPoint(newJunctionId, 0), conn.destination);
				replacement.removeConnection(pauseGuard, conn);
				replacement.makeConnection(pauseGuard, newConnection);
			}
		}
	}

	JunctionFloodFillResult junctionFloodFill(middle_id_t junctionId) {
		JunctionFloodFillResult result;
		std::set<middle_id_t> visited;
		std::set<EvalConnectionPoint> visitedOutputs;
		std::queue<middle_id_t> queue;
		queue.push(junctionId);
		visited.insert(junctionId);
		while (!queue.empty()) {
			middle_id_t currentId = queue.front();
			queue.pop();
			result.junctionIds.push_back(currentId);
			std::vector<EvalConnection> outputs = simulatorOptimizer.getOutputs(currentId);
			std::vector<EvalConnection> inputs = simulatorOptimizer.getInputs(currentId);
			for (const auto& output : outputs) {
				if (visited.contains(output.destination.gateId)) {
					continue;
				}
				GateType outputGateType = simulatorOptimizer.getGateType(output.destination.gateId);
				if (outputGateType == GateType::JUNCTION) {
					queue.push(output.destination.gateId);
					visited.insert(output.destination.gateId);
					continue;
				}
				result.inputsPullingFromJunctions.push_back(output);
			}
			for (const auto& input : inputs) {
				if (visited.contains(input.source.gateId)) {
					continue;
				}
				GateType inputGateType = simulatorOptimizer.getGateType(input.source.gateId);
				if (inputGateType == GateType::JUNCTION) {
					queue.push(input.source.gateId);
					visited.insert(input.source.gateId);
					continue;
				}
				// not a junction going into a junction
				if (visitedOutputs.contains(input.source)) {
					continue;
				}
				visitedOutputs.insert(input.source);
				result.outputsGoingIntoJunctions.push_back(input.source);
				std::vector<EvalConnection> nodeOutputs = simulatorOptimizer.getOutputs(input.source.gateId);
				for (const auto& nodeOutput : nodeOutputs) {
					if (nodeOutput.source.portId != input.source.portId) {
						continue; // only consider outputs from the same port
					}
					GateType nodeOutputGateType = simulatorOptimizer.getGateType(nodeOutput.destination.gateId);
					if (nodeOutputGateType == GateType::JUNCTION) {
						if (visited.contains(nodeOutput.destination.gateId)) {
							continue;
						}
						queue.push(nodeOutput.destination.gateId);
						visited.insert(nodeOutput.destination.gateId);
					} else {
						result.connectionsToReroute.push_back(nodeOutput);
					}
				}
			}
		}
		return result;
	}
};

#endif // replacer_h