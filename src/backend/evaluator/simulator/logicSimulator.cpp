#include "logicSimulator.h"

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
	gates.emplace(gateId, SimulatorGate { blockType, {} });
	logInfo("Added gate {} of type {}", "LogicSimulator::addGate", gateId.get(), blocktype_to_string(blockType));
}

void LogicSimulator::removeGate(eval_gate_id gateId) {
	assert(gates.contains(gateId) && "Gate does not exist in LogicSimulator");
	removeAllGateConnections(gateId);
	gates.erase(gateId);
	logInfo("Removed gate {}", "LogicSimulator::removeGate", gateId.get());
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
		(gateAPortDirection == PortDirection::OUTPUT && (gateBPortDirection == PortDirection::INPUT || gateBPortDirection == PortDirection::BIDDIR)) ||
		(gateAPortDirection == PortDirection::INPUT && (gateBPortDirection == PortDirection::OUTPUT || gateBPortDirection == PortDirection::BIDDIR)) ||
		(gateAPortDirection == PortDirection::BIDDIR && (gateBPortDirection == PortDirection::INPUT || gateBPortDirection == PortDirection::OUTPUT))
	);

	std::unordered_map<EvalConnectionPoint, unsigned int>& gateAConnectionsFromPort = gateA.getConnectionsFromPort(evalConnection.connectionPointA.connectionEndId);
	std::unordered_map<EvalConnectionPoint, unsigned int>& gateBConnectionsFromPort = gateB.getConnectionsFromPort(evalConnection.connectionPointB.connectionEndId);

	// validate single-connection ports

	unsigned int currentWeight = 0;
	if (gateAConnectionsFromPort.contains(evalConnection.connectionPointB)) {
		currentWeight = gateAConnectionsFromPort.at(evalConnection.connectionPointB);
	}
	assert(
		(!gateBConnectionsFromPort.contains(evalConnection.connectionPointA)) ||
		(currentWeight == gateBConnectionsFromPort.at(evalConnection.connectionPointA))
	);
	int newWeight = static_cast<int>(currentWeight) + weight;
	if (gateAPortInfo.limitedToOneConnection) {
		assert(newWeight <= 1 && "Port on gate A is limited to one connection");
	}
	assert(newWeight >= 0 && "Connection weight cannot be negative");
	logInfo(
		"{} connection between gate {} port {} and gate {} port {} (new weight {})",
		"LogicSimulator::addConnection",
		weight >= 0 ? "Added" : "Removed",
		evalConnection.connectionPointA.gateId.get(),
		evalConnection.connectionPointA.connectionEndId.get(),
		evalConnection.connectionPointB.gateId.get(),
		evalConnection.connectionPointB.connectionEndId.get(),
		newWeight
	);
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
	logInfo("Ended edit session", "LogicSimulator::endEdit");
}

void LogicSimulator::resetStates() {}
void LogicSimulator::setState(simulator_state_index_t simulatorStateIndex, logic_state_t state) {}
logic_state_t LogicSimulator::getState(simulator_state_index_t simulatorStateIndex) const { return logic_state_t::UNDEFINED; }
std::vector<logic_state_t> LogicSimulator::getStates(const std::vector<simulator_state_index_t>& simulatorStateIndices) const {
	// Simple implementation using getState for each index
	// Future implementation will only lock once for efficiency
	std::vector<logic_state_t> states;
	for (const auto& index : simulatorStateIndices) {
		states.push_back(getState(index));
	}
	return states;
}

std::optional<simulator_state_index_t> LogicSimulator::getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const {
	return simulator_state_index_t(1);
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
				return PortInfo { PortDirection::BIDDIR, false };
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
}
