#include "logicSimulator.h"
#include "gateGroup.h"
#include "util/algorithm.h"

void LogicSimulator::addGate(eval_gate_id gateId, BlockType blockType) {
	assert(
		blockType == BlockType::AND ||
		blockType == BlockType::OR ||
		blockType == BlockType::XOR ||
		blockType == BlockType::NAND ||
		blockType == BlockType::NOR ||
		blockType == BlockType::XNOR ||
		blockType == BlockType::BUFFER ||
		blockType == BlockType::NOT ||
		blockType == BlockType::JUNCTION ||
		blockType == BlockType::JUNCTION_L ||
		blockType == BlockType::JUNCTION_H ||
		blockType == BlockType::JUNCTION_X ||
		blockType == BlockType::BUTTON ||
		blockType == BlockType::TICK_BUTTON ||
		blockType == BlockType::SWITCH ||
		blockType == BlockType::CONSTANT_OFF ||
		blockType == BlockType::CONSTANT_ON ||
		blockType == BlockType::CONSTANT_Z ||
		blockType == BlockType::CONSTANT_X ||
		blockType == BlockType::TRISTATE_BUFFER
	);
	assert(!gates.contains(gateId) && "Gate already exists in LogicSimulator");
	gates.emplace(gateId, SimulatorGate { gateId, blockType, {} });
}

void LogicSimulator::removeGate(eval_gate_id gateId) {
	assert(gates.contains(gateId) && "Gate does not exist in LogicSimulator");
	removeAllGateConnections(gateId);
	gates.erase(gateId);
}

void LogicSimulator::addConnection(const EvalConnection& evalConnection, int weight) {
	SimulatorGate& gateA = gates.at(evalConnection.connectionPointA.gateId);
	SimulatorGate& gateB = gates.at(evalConnection.connectionPointB.gateId);
	BlockType gateAType = gateA.type;
	BlockType gateBType = gateB.type;

	PortInfo gateAPortInfo = getPortInfo(gateAType, evalConnection.connectionPointA.connectionEndId);
	PortInfo gateBPortInfo = getPortInfo(gateBType, evalConnection.connectionPointB.connectionEndId);
	PortDirection gateAPortDirection = gateAPortInfo.direction;
	PortDirection gateBPortDirection = gateBPortInfo.direction;

	// validate connection directions

	assert(
		(gateAPortDirection == PortDirection::OUTPUT && (gateBPortDirection == PortDirection::INPUT || gateBPortDirection == PortDirection::BIDIRECTIONAL)) ||
		(gateAPortDirection == PortDirection::INPUT && (gateBPortDirection == PortDirection::OUTPUT || gateBPortDirection == PortDirection::BIDIRECTIONAL)) ||
		(gateAPortDirection == PortDirection::BIDIRECTIONAL && (gateBPortDirection == PortDirection::INPUT || gateBPortDirection == PortDirection::OUTPUT))
	);

	std::unordered_map<EvalConnectionPoint, unsigned int>& gateAConnectionsFromPort = gateA.getConnectionsFromPort(evalConnection.connectionPointA.connectionEndId);
	std::unordered_map<EvalConnectionPoint, unsigned int>& gateBConnectionsFromPort = gateB.getConnectionsFromPort(evalConnection.connectionPointB.connectionEndId);

	// validate single-connection ports

	unsigned int oldWeight = 0;
	if (gateAConnectionsFromPort.contains(evalConnection.connectionPointB)) {
		oldWeight = gateAConnectionsFromPort.at(evalConnection.connectionPointB);
	}
	assert(
		(!gateBConnectionsFromPort.contains(evalConnection.connectionPointA)) ||
		(oldWeight == gateBConnectionsFromPort.at(evalConnection.connectionPointA))
	);
	int newWeight = static_cast<int>(oldWeight) + weight;

	BlockType destinationGateType = BlockType::NONE;
	// if (gateAPortDirection == PortDirection::OUTPUT) {
	// 	destinationGateType = gateBType;
	// } else if (gateAPortDirection == PortDirection::INPUT) {
	// 	destinationGateType = gateAType;
	// } else { // gateAPortDirection == PortDirection::BIDIRECTIONAL
	// 	if (gateBPortDirection == PortDirection::OUTPUT) {
	// 		destinationGateType = gateAType;
	// 	} else if (gateBPortDirection == PortDirection::INPUT) {
	// 		destinationGateType = gateBType;
	// 	} else { // gateBPortDirection == PortDirection::BIDIRECTIONAL
	// 		assert(false && "Cannot determine destination gate type for connection between two bidirectional ports");
	// 	}
	// }
	ConnectionDirection connectionDirection = getConnectionDirection(evalConnection);
	if (connectionDirection == ConnectionDirection::AtoB) {
		destinationGateType = gateBType;
	} else if (connectionDirection == ConnectionDirection::BtoA) {
		destinationGateType = gateAType;
	} else {
		assert(false && "Invalid connection direction");
	}

	if (destinationGateType == BlockType::XOR || destinationGateType == BlockType::XNOR) {
		newWeight = newWeight % 2; // XOR/XNOR gates only care about odd/even number of connections
	}

	if (gateAPortInfo.limitedToOneConnection) {
		assert(newWeight <= 1 && "Port on gate A is limited to one connection");
	}
	assert(newWeight >= 0 && "Connection weight cannot be negative");
	if (newWeight == 0) {
		gateAConnectionsFromPort.erase(evalConnection.connectionPointB);
		gateBConnectionsFromPort.erase(evalConnection.connectionPointA);
	} else {
		gateAConnectionsFromPort[evalConnection.connectionPointB] = static_cast<unsigned int>(newWeight);
		gateBConnectionsFromPort[evalConnection.connectionPointA] = static_cast<unsigned int>(newWeight);
	}
}

void LogicSimulator::removeConnection(const EvalConnection& evalConnection, int weight) {
	addConnection(evalConnection, -weight);
}

void LogicSimulator::endEdit() {
	LogicGroupRunner::EditingGuard editingGuard = logicGroupRunner.getEditingGuard();
	std::unordered_map<gate_group_id_t, CompiledGateGroup> compiledGroups = compileGroups();
	std::unordered_map<gate_group_id_t, LinkedGateGroup> linkedGroups = groupLinker.linkGroups(compiledGroups);
	logicGroupRunner.setGroups(linkedGroups);
}

void LogicSimulator::resetStates() {}

void LogicSimulator::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {}

logic_state_t LogicSimulator::getState(simulator_state_reference simulatorStateIndex) const {
	if (simulatorStateIndex == simulator_state_reference(0)) {
		return logic_state_t::LOW;
	} else if (simulatorStateIndex == simulator_state_reference(1)) {
		return logic_state_t::HIGH;
	} else if (simulatorStateIndex == simulator_state_reference(2)) {
		return logic_state_t::FLOATING;
	} else if (simulatorStateIndex == simulator_state_reference(3)) {
		return logic_state_t::UNDEFINED;
	} else {
		LogicGroupRunner::ReadingGuard readingGuard = logicGroupRunner.getReadingGuard();
		return getRunnerState_noMux(simulatorStateIndex);
	}
}

std::vector<logic_state_t> LogicSimulator::getStates(const std::vector<simulator_state_reference>& simulatorStateIndices) const {
	std::optional<LogicGroupRunner::ReadingGuard> readingGuardOpt;
	std::vector<logic_state_t> states;
	for (const auto& index : simulatorStateIndices) {
		if (index == simulator_state_reference(0)) {
			states.push_back(logic_state_t::LOW);
			continue;
		} else if (index == simulator_state_reference(1)) {
			states.push_back(logic_state_t::HIGH);
			continue;
		} else if (index == simulator_state_reference(2)) {
			states.push_back(logic_state_t::FLOATING);
			continue;
		} else if (index == simulator_state_reference(3)) {
			states.push_back(logic_state_t::UNDEFINED);
			continue;
		}
		if (!readingGuardOpt.has_value()) {
			readingGuardOpt.emplace(logicGroupRunner.getReadingGuard());
		}
		states.push_back(getRunnerState_noMux(index));
	}
	return states;
}

logic_state_t LogicSimulator::getRunnerState_noMux(simulator_state_reference simulatorStateIndex) const {
	return logicGroupRunner.getState(simulatorStateIndex);
}

std::optional<simulator_state_reference> LogicSimulator::getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const {
	return simulator_state_reference(2);
}
void LogicSimulator::setRunning(bool running) {}
bool LogicSimulator::isRunning() const { return false; }
void LogicSimulator::setRealistic(bool realistic) {}
bool LogicSimulator::isRealistic() const { return false; }
void LogicSimulator::setUseTickrateLimiter(bool useTickrate) {}
bool LogicSimulator::getUseTickrateLimiter() const { return false; }
void LogicSimulator::setTargetTickrate(double tickrate) {}
double LogicSimulator::getTargetTickrate() const { return 40.0; }
double LogicSimulator::getAverageTickrate() const { return 0.0; }
void LogicSimulator::addSprint(unsigned int nTicks) {}
unsigned int LogicSimulator::getSprintCount() const { return 0; }
void LogicSimulator::waitForSprintComplete() {}
bool LogicSimulator::stepBack() { return false; }
bool LogicSimulator::stepForward() { return false; }
bool LogicSimulator::skipBack() { return false; }
bool LogicSimulator::skipForward() { return false; }
bool LogicSimulator::isViewingReplay() const { return false; }
nlohmann::json LogicSimulator::dumpState() const { return nlohmann::json::object(); }

// helpers

void LogicSimulator::removeAllGateConnections(eval_gate_id gateId) {
	SimulatorGate& simulatorGate = gates.at(gateId);
	std::vector<std::pair<EvalConnection, unsigned int>> connectionsToRemove;
	for (const auto& [connectionEndId, connectionsMap] : simulatorGate.connections) {
		for (const auto& [evalConnectionPoint, weight] : connectionsMap) {
			if (evalConnectionPoint.gateId == gateId && evalConnectionPoint.connectionEndId > connectionEndId) {
				continue; // to avoid processing the same connection twice, we only process it from one end
			}
			connectionsToRemove.emplace_back(EvalConnection(EvalConnectionPoint(gateId, connectionEndId), evalConnectionPoint), weight);
		}
	}
	for (const auto& [evalConnection, weight] : connectionsToRemove) {
		removeConnection(evalConnection, weight);
	}
}

LogicSimulator::PortInfo LogicSimulator::getPortInfo(BlockType blockType, connection_end_id_t connectionEndId) {
	switch (blockType) {
		case BlockType::AND:
		case BlockType::OR:
		case BlockType::XOR:
		case BlockType::NAND:
		case BlockType::NOR:
		case BlockType::XNOR:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, false };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for gate block type");
			break;
		case BlockType::BUFFER:
		case BlockType::NOT:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for buffer/not block type");
			break;
		case BlockType::BUTTON:
		case BlockType::TICK_BUTTON:
		case BlockType::SWITCH:
		case BlockType::CONSTANT_OFF:
		case BlockType::CONSTANT_ON:
		case BlockType::CONSTANT_Z:
		case BlockType::CONSTANT_X:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for constant block type");
			break;
		case BlockType::JUNCTION:
		case BlockType::JUNCTION_L:
		case BlockType::JUNCTION_H:
		case BlockType::JUNCTION_X:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::BIDIRECTIONAL, false };
			}
			assert(false && "Invalid connection end ID for junction block type");
			break;
		case BlockType::TRISTATE_BUFFER:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 2) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for tristate buffer block type");
			break;
		default:
			assert(false && "Unknown block type in getPortInfo");
	}
	assert(false && "Unreachable code in getPortInfo");
	return PortInfo { PortDirection::INPUT, false };
}

BlockType LogicSimulator::getBlockType(eval_gate_id gateId) const {
	return gates.at(gateId).type;
}

LogicSimulator::PortDirection LogicSimulator::getConnectionPointDirection(const EvalConnectionPoint& evalConnectionPoint) const {
	return getPortDirection(getBlockType(evalConnectionPoint.gateId), evalConnectionPoint.connectionEndId);
}

LogicSimulator::ConnectionDirection LogicSimulator::getConnectionDirection(
	const EvalConnection& evalConnection
) const {
	PortDirection portADirection = getConnectionPointDirection(evalConnection.connectionPointA);
	if (portADirection == PortDirection::OUTPUT) {
		return ConnectionDirection::AtoB;
	} else if (portADirection == PortDirection::INPUT) {
		return ConnectionDirection::BtoA;
	}

	PortDirection portBDirection = getConnectionPointDirection(evalConnection.connectionPointB);
	if (portBDirection == PortDirection::OUTPUT) {
		return ConnectionDirection::BtoA;
	} else if (portBDirection == PortDirection::INPUT) {
		return ConnectionDirection::AtoB;
	}

	assert(false && "Both ports are bidirectional in getConnectionDirection");
	return ConnectionDirection::AtoB; // to silence compiler warning
}

std::unordered_map<gate_group_id_t, CompiledGateGroup> LogicSimulator::compileGroups() const {
	std::unordered_map<gate_group_id_t, CompiledGateGroup> compiledGroups;
	IdProvider<gate_group_id_t> newGroupIdProvider { 0 };
	std::unordered_set<eval_gate_id> ungroupedGates = {};
	std::unordered_set<eval_gate_id> ungroupedJunctions = {};
	for (const auto& [gateId, simulatorGate] : gates) {
		if (isJunction(gateId)) {
			ungroupedJunctions.insert(gateId);
		} else {
			ungroupedGates.insert(gateId);
		}
	}
	while (ungroupedGates.size() != 0) {
		logInfo("{} ungrouped gates remaining", "LogicSimulator::compileGroups", ungroupedGates.size());
		eval_gate_id gateId = *ungroupedGates.begin();
		assert(!isJunction(gateId) && "Expected gate, got junction in compileGroups");
		// walk back then walk forward
		std::unordered_set<EvalConnectionPoint> backPlane;
		std::unordered_set<EvalConnectionPoint> visitedOutputConnectionPoints;

		ungroupedGates.erase(gateId);
		std::unordered_set<eval_gate_id> groupGateIds = { gateId };
		std::vector<eval_gate_id> toVisit = { gateId };
		while (toVisit.size() > 0) {
			eval_gate_id currentGateId = toVisit.back();
			toVisit.pop_back();
			const SimulatorGate& currentGate = gates.at(currentGateId);
			// step 1. walk back through the circuit and create a "back plane" of all the output connection points that contribute to calculating the state of the current gate
			for (const auto& [connectionEndId, connectionsMap] : currentGate.connections) {
				PortDirection portDirection = getPortDirection(currentGate.type, connectionEndId);
				if (portDirection != PortDirection::INPUT) continue; // we only want to walk back
				for (const auto& [evalConnectionPoint, weight] : connectionsMap) {
					if (weight == 0) continue;
					eval_gate_id otherGateId = evalConnectionPoint.gateId;
					if (visitedOutputConnectionPoints.contains(evalConnectionPoint)) continue;
					visitedOutputConnectionPoints.insert(evalConnectionPoint);
					if (isJunction(otherGateId)) {
						// walk back again from the junction because junctions are dumb and arent real and i wanna kms
						const SimulatorGate& junctionGate = gates.at(otherGateId);
						connection_end_id_t connectionEndId = connection_end_id_t(0);
						if (!junctionGate.hasConnectionsFromPort(connectionEndId)) continue;
						const std::unordered_map<EvalConnectionPoint, unsigned int>& junctionConnections = junctionGate.getConnectionsFromPort(connectionEndId);
						for (const auto& [junctionOtherGateConnectionPoint, junctionConnectionWeight] : junctionConnections) {
							if (junctionConnectionWeight == 0) continue;
							PortDirection junctionOtherGatePortDirection = getConnectionPointDirection(junctionOtherGateConnectionPoint);
							if (junctionOtherGatePortDirection != PortDirection::OUTPUT) continue; // we only want to walk back
							if (visitedOutputConnectionPoints.contains(junctionOtherGateConnectionPoint)) continue;
							visitedOutputConnectionPoints.insert(junctionOtherGateConnectionPoint);
							backPlane.insert(junctionOtherGateConnectionPoint);
						}
					} else {
						backPlane.insert(evalConnectionPoint);
					}
				}
			}
			// step 2. walk forward through the circuit and note down any gates that the current back plane contributes to calculating the state of
			for (const auto& backPlaneConnectionPoint : backPlane) {
				// go through all the outputs of the connection point
				eval_gate_id backPlaneGateId = backPlaneConnectionPoint.gateId;
				const SimulatorGate& backPlaneGate = gates.at(backPlaneGateId);
				if (!backPlaneGate.hasConnectionsFromPort(backPlaneConnectionPoint.connectionEndId)) continue;
				const std::unordered_map<EvalConnectionPoint, unsigned int>& backPlaneGateConnections = backPlaneGate.getConnectionsFromPort(backPlaneConnectionPoint.connectionEndId);
				for (const auto& [otherConnectionPoint, weight] : backPlaneGateConnections) {
					if (weight == 0) continue;
					eval_gate_id otherGateId = otherConnectionPoint.gateId;
					if (groupGateIds.contains(otherGateId)) continue; // already in the group
					if (isJunction(otherGateId)) {
						groupGateIds.insert(otherGateId);
						ungroupedJunctions.erase(otherGateId);
						// walk forward again because the instant value calculated for the junction will be used to calculate the state of the gates the junction connects into
						const SimulatorGate& junctionGate = gates.at(otherGateId);
						connection_end_id_t connectionEndId = connection_end_id_t(0);
						if (!junctionGate.hasConnectionsFromPort(connectionEndId)) continue;
						const std::unordered_map<EvalConnectionPoint, unsigned int>& junctionConnections = junctionGate.getConnectionsFromPort(connectionEndId);
						for (const auto& [junctionOtherGateConnectionPoint, junctionConnectionWeight] : junctionConnections) {
							if (junctionConnectionWeight == 0) continue;
							PortDirection junctionOtherGatePortDirection = getConnectionPointDirection(junctionOtherGateConnectionPoint);
							if (junctionOtherGatePortDirection != PortDirection::INPUT) continue; // we only want to walk forward
							eval_gate_id junctionOtherGateId = junctionOtherGateConnectionPoint.gateId;
							if (groupGateIds.contains(junctionOtherGateId)) continue; // already in the group
							groupGateIds.insert(junctionOtherGateId);
							ungroupedGates.erase(junctionOtherGateId);
							toVisit.push_back(junctionOtherGateId);
						}
					} else {
						groupGateIds.insert(otherGateId);
						ungroupedGates.erase(otherGateId);
						toVisit.push_back(otherGateId);
					}
				}
			}
		}
		gate_group_id_t gateGroupId = newGroupIdProvider.getNewId();
		std::vector<SimulatorGate> groupedGates;
		std::vector<EvalConnectionPoint> pullConnectionPoints;
		logInfo("----------------------------------------------------------", "LogicSimulator::compileGroups");
		logInfo("Group {}", "LogicSimulator::compileGroups", gateGroupId);
		logInfo("Group gates: {}", "LogicSimulator::compileGroups", to_string(groupGateIds));
		logInfo("Visited output connection points: {}", "LogicSimulator::compileGroups", to_string(visitedOutputConnectionPoints));
		for (eval_gate_id groupGateId : groupGateIds) {
			groupedGates.push_back(gates.at(groupGateId));
		}
		for (const EvalConnectionPoint& visitedOutputConnectionPoint : visitedOutputConnectionPoints) {
			if (!groupGateIds.contains(visitedOutputConnectionPoint.gateId)) {
				pullConnectionPoints.push_back(visitedOutputConnectionPoint);
			}
		}
		compiledGroups[gateGroupId] = CompiledGateGroup(groupedGates, pullConnectionPoints);
	}
	if (ungroupedJunctions.size() > 0) {
		std::vector<SimulatorGate> junctionGates;
		for (eval_gate_id junctionGateId : ungroupedJunctions) {
			junctionGates.push_back(gates.at(junctionGateId));
		}
		CompiledGateGroup junctionGroup(junctionGates, {}); // no pull connection points because these junctions are only going to be computed on getState;
		compiledGroups[newGroupIdProvider.getNewId()] = junctionGroup;
	}
	return compiledGroups;
}
