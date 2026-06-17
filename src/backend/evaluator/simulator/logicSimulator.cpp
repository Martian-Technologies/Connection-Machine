#include "logicSimulator.h"
#include "gateGroup.h"
#include "util/algorithm.h"
#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

using namespace SimBlockData;

namespace {
	constexpr std::uint64_t GROUP_NODE_FLAG = 0;
	constexpr std::uint64_t GATE_NODE_FLAG = 1ULL << 63;

	std::uint64_t makeGroupNode(gate_group_id_t groupId) {
		return GROUP_NODE_FLAG | groupId.get();
	}

	std::uint64_t makeGateNode(eval_gate_id gateId) {
		return GATE_NODE_FLAG | gateId.get();
	}

	class DisjointSet {
	public:
		void add(std::uint64_t node) {
			if (parent.contains(node)) {
				return;
			}
			parent[node] = node;
			sizes[node] = 1;
		}

		std::uint64_t find(std::uint64_t node) {
			add(node);
			std::uint64_t root = node;
			while (parent.at(root) != root) {
				root = parent.at(root);
			}
			while (parent.at(node) != node) {
				std::uint64_t next = parent.at(node);
				parent[node] = root;
				node = next;
			}
			return root;
		}

		void unite(std::uint64_t a, std::uint64_t b) {
			std::uint64_t rootA = find(a);
			std::uint64_t rootB = find(b);
			if (rootA == rootB) {
				return;
			}
			if (sizes.at(rootA) < sizes.at(rootB)) {
				std::swap(rootA, rootB);
			}
			parent[rootB] = rootA;
			sizes[rootA] += sizes[rootB];
		}

	private:
		std::unordered_map<std::uint64_t, std::uint64_t> parent;
		std::unordered_map<std::uint64_t, unsigned int> sizes;
	};

	struct DeferredGroupComponent {
		std::vector<gate_group_id_t> groups;
		std::vector<eval_gate_id> gates;
	};
}

void LogicSimulator::addGate(eval_gate_id gateId, BlockType blockType) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
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
	ungroupedGates.insert(gateId);
}

void LogicSimulator::removeGate(eval_gate_id gateId) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	assert(gates.contains(gateId) && "Gate does not exist in LogicSimulator");
	deletedGatesInCurrentEdit.insert(gateId);
	removeAllGateConnections(gateId);
	removeGateFromGroup(gateId);
	gates.erase(gateId);
}

void LogicSimulator::addConnection(const EvalConnection& evalConnection, int weight) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
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

	assert(newWeight >= 0 && "Connection weight cannot be negative");
	if (newWeight == 0) {
		gateAConnectionsFromPort.erase(evalConnection.connectionPointB);
		gateBConnectionsFromPort.erase(evalConnection.connectionPointA);
		if (gateAPortDirection == PortDirection::BIDIRECTIONAL) {
			gateA.directionsOfBidirectionalPorts[evalConnection.connectionPointA.connectionEndId].erase(evalConnection.connectionPointB);
		} else if (gateBPortDirection == PortDirection::BIDIRECTIONAL) {
			gateB.directionsOfBidirectionalPorts[evalConnection.connectionPointB.connectionEndId].erase(evalConnection.connectionPointA);
		}
	} else {
		gateAConnectionsFromPort[evalConnection.connectionPointB] = static_cast<unsigned int>(newWeight);
		gateBConnectionsFromPort[evalConnection.connectionPointA] = static_cast<unsigned int>(newWeight);
		if (oldWeight == 0) {
			if (gateAPortDirection == PortDirection::BIDIRECTIONAL) {
				InputOutput direction = (gateBPortDirection == PortDirection::OUTPUT) ? InputOutput::INPUT : InputOutput::OUTPUT;
				gateA.directionsOfBidirectionalPorts[evalConnection.connectionPointA.connectionEndId][evalConnection.connectionPointB] = direction;
			} else if (gateBPortDirection == PortDirection::BIDIRECTIONAL) {
				InputOutput direction = (gateAPortDirection == PortDirection::OUTPUT) ? InputOutput::INPUT : InputOutput::OUTPUT;
				gateB.directionsOfBidirectionalPorts[evalConnection.connectionPointB.connectionEndId][evalConnection.connectionPointA] = direction;
			}
		}
	}
	if (newWeight > 0) {
#ifdef TRACY_PROFILER
		ZoneScopedN("newWeight > 0");
#endif
		if (oldWeight == 0) {
			pendingAddedConnections.insert(evalConnection);
			markGateGroupDirty(evalConnection.connectionPointA.gateId);
			markGateGroupDirty(evalConnection.connectionPointB.gateId);
		}
	} else {
		pendingAddedConnections.erase(evalConnection);
		markGateGroupDirty(evalConnection.connectionPointA.gateId);
		markGateGroupDirty(evalConnection.connectionPointB.gateId);
	}
}

void LogicSimulator::removeConnection(const EvalConnection& evalConnection, int weight) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	addConnection(evalConnection, -weight);
}

void LogicSimulator::endEdit() {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	LogicGroupRunner::EditingGuard editingGuard = logicGroupRunner.getEditingGuard();
	// materializePendingGroupMerges();
	// refreshDirtyGroups();
	compiledGroups = compileGroups();
	gateIdToGroupId.clear();
	groupIdProvider.reset();
	for (const auto& [groupId, group] : compiledGroups) {
		groupIdProvider.getNewId(groupId);
		for (const SimulatorGate& gate : group.gates) {
			gateIdToGroupId[gate.id] = groupId;
		}
	}

	std::unordered_map<gate_group_id_t, LinkedGateGroup> linkedGroups = groupLinker.linkGroups(compiledGroups);
	logicGroupRunner.setGroups(linkedGroups, deletedGatesInCurrentEdit);
	deletedGatesInCurrentEdit.clear();
	pendingAddedConnections.clear();
	ungroupedGates.clear();
	dirtyGroups.clear();
}

void LogicSimulator::resetStates() {}

void LogicSimulator::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	logicGroupRunner.setState(simulatorStateIndex, state);
}

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
		LogicGroupRunner::StateReadingGuard stateReadingGuard = logicGroupRunner.getStateReadingGuard();
		return getRunnerState_noMux(simulatorStateIndex);
	}
}

std::vector<logic_state_t> LogicSimulator::getStates(const std::vector<simulator_state_reference>& simulatorStateIndices) const {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::optional<LogicGroupRunner::StateReadingGuard> stateReadingGuardOpt;
	std::vector<logic_state_t> states;
	states.reserve(simulatorStateIndices.size());
	for (const auto& index : simulatorStateIndices) {
#ifdef TRACY_PROFILER
		ZoneScopedN("getState loop");
#endif
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
		if (!stateReadingGuardOpt.has_value()) {
#ifdef TRACY_PROFILER
			ZoneScopedN("acquire state reading guard");
#endif
			stateReadingGuardOpt.emplace(logicGroupRunner.getStateReadingGuard());
		}
		states.push_back(getRunnerState_noMux(index));
	}
	return states;
}

logic_state_t LogicSimulator::getRunnerState_noMux(simulator_state_reference simulatorStateIndex) const {
	return logicGroupRunner.getState(simulatorStateIndex);
}

std::optional<simulator_state_reference> LogicSimulator::getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const {
	simulator_state_reference simulatorStateIndex = logicGroupRunner.getSimulatorStateIndex(evalConnectionPoint);
	if (simulatorStateIndex.get() < 4) {
		return std::nullopt;
	}
	return simulatorStateIndex;
}
void LogicSimulator::setRunning(bool running) { logicGroupRunner.setRunning(running); }
bool LogicSimulator::isRunning() const { return logicGroupRunner.isRunning(); }
void LogicSimulator::setRealistic(bool realistic) { logicGroupRunner.setRealistic(realistic); }
bool LogicSimulator::isRealistic() const { return logicGroupRunner.isRealistic(); }
void LogicSimulator::setUseTickrateLimiter(bool useTickrate) { logicGroupRunner.setUseTickrateLimiter(useTickrate); }
bool LogicSimulator::getUseTickrateLimiter() const { return logicGroupRunner.getUseTickrateLimiter(); }
void LogicSimulator::setTargetTickrate(double tickrate) { logicGroupRunner.setTargetTickrate(tickrate); }
double LogicSimulator::getTargetTickrate() const { return logicGroupRunner.getTargetTickrate(); }
double LogicSimulator::getAverageTickrate() const { return logicGroupRunner.getAverageTickrate(); }
void LogicSimulator::addSprint(unsigned int nTicks) { logicGroupRunner.addSprint(nTicks); }
unsigned int LogicSimulator::getSprintCount() const { return logicGroupRunner.getSprintCount(); }
void LogicSimulator::waitForSprintComplete() { logicGroupRunner.waitForSprintComplete(); }
bool LogicSimulator::stepBack() { return logicGroupRunner.stepBack(); }
bool LogicSimulator::stepForward() { return logicGroupRunner.stepForward(); }
bool LogicSimulator::skipBack() { return logicGroupRunner.skipBack(); }
bool LogicSimulator::skipForward() { return logicGroupRunner.skipForward(); }
bool LogicSimulator::isViewingReplay() const { return logicGroupRunner.isViewingReplay(); }
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

BlockType LogicSimulator::getBlockType(eval_gate_id gateId) const {
	return gates.at(gateId).type;
}

void LogicSimulator::createGroupForGate(eval_gate_id gateId) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	gate_group_id_t groupId = groupIdProvider.getNewId();
	auto inserted = gateIdToGroupId.emplace(gateId, groupId);
	assert(inserted.second && "Gate already belongs to a simulator group");
	compiledGroups.emplace(groupId, CompiledGateGroup({ gates.at(gateId) }, {}));
	dirtyGroups.insert(groupId);
}

void LogicSimulator::markGateGroupDirty(eval_gate_id gateId) {
	auto iter = gateIdToGroupId.find(gateId);
	if (iter == gateIdToGroupId.end()) {
		return;
	}
	dirtyGroups.insert(iter->second);
}

void LogicSimulator::mergeGroups(gate_group_id_t groupIdA, gate_group_id_t groupIdB) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (groupIdA == groupIdB) {
		dirtyGroups.insert(groupIdA);
		return;
	}

	auto groupAIter = compiledGroups.find(groupIdA);
	auto groupBIter = compiledGroups.find(groupIdB);
	assert(groupAIter != compiledGroups.end() && groupBIter != compiledGroups.end());

	gate_group_id_t targetGroupId = groupIdA;
	gate_group_id_t sourceGroupId = groupIdB;
	if (groupBIter->second.gates.size() > groupAIter->second.gates.size()) {
		targetGroupId = groupIdB;
		sourceGroupId = groupIdA;
	}

	CompiledGateGroup& targetGroup = compiledGroups.at(targetGroupId);
	CompiledGateGroup& sourceGroup = compiledGroups.at(sourceGroupId);
	targetGroup.gates.reserve(targetGroup.gates.size() + sourceGroup.gates.size());
	for (const SimulatorGate& gate : sourceGroup.gates) {
		gateIdToGroupId[gate.id] = targetGroupId;
		targetGroup.gates.push_back(gate);
	}

	compiledGroups.erase(sourceGroupId);
	dirtyGroups.erase(sourceGroupId);
	dirtyGroups.insert(targetGroupId);
}

void LogicSimulator::addGateToGroup(eval_gate_id gateId, gate_group_id_t groupId) {
	auto inserted = gateIdToGroupId.emplace(gateId, groupId);
	assert(inserted.second && "Gate already belongs to a simulator group");
	compiledGroups.at(groupId).gates.push_back(gates.at(gateId));
	ungroupedGates.erase(gateId);
	dirtyGroups.insert(groupId);
}

void LogicSimulator::removeGateFromGroup(eval_gate_id gateId) {
	if (ungroupedGates.erase(gateId) > 0) {
		return;
	}

	auto groupIter = gateIdToGroupId.find(gateId);
	if (groupIter == gateIdToGroupId.end()) {
		return;
	}

	gate_group_id_t groupId = groupIter->second;
	gateIdToGroupId.erase(groupIter);

	auto compiledGroupIter = compiledGroups.find(groupId);
	assert(compiledGroupIter != compiledGroups.end());
	std::erase_if(compiledGroupIter->second.gates, [gateId](const SimulatorGate& gate) {
		return gate.id == gateId;
	});
	if (compiledGroupIter->second.gates.empty()) {
		compiledGroups.erase(compiledGroupIter);
		dirtyGroups.erase(groupId);
	} else {
		dirtyGroups.insert(groupId);
	}
}

void LogicSimulator::materializePendingGroupMerges() {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (ungroupedGates.empty() && pendingAddedConnections.empty()) {
		return;
	}

	DisjointSet disjointSet;
	std::unordered_set<gate_group_id_t> touchedGroups;

	for (eval_gate_id gateId : ungroupedGates) {
		if (gates.contains(gateId)) {
			disjointSet.add(makeGateNode(gateId));
		}
	}

	auto nodeForGate = [&](eval_gate_id gateId) -> std::optional<std::uint64_t> {
		if (!gates.contains(gateId)) {
			return std::nullopt;
		}
		auto groupIter = gateIdToGroupId.find(gateId);
		if (groupIter != gateIdToGroupId.end()) {
			touchedGroups.insert(groupIter->second);
			return makeGroupNode(groupIter->second);
		}
		if (ungroupedGates.contains(gateId)) {
			return makeGateNode(gateId);
		}
		return std::nullopt;
	};

	for (const EvalConnection& connection : pendingAddedConnections) {
		const auto nodeA = nodeForGate(connection.connectionPointA.gateId);
		const auto nodeB = nodeForGate(connection.connectionPointB.gateId);
		if (!nodeA.has_value() || !nodeB.has_value()) {
			continue;
		}
		disjointSet.unite(nodeA.value(), nodeB.value());
	}

	std::unordered_map<std::uint64_t, DeferredGroupComponent> components;
	for (eval_gate_id gateId : ungroupedGates) {
		if (!gates.contains(gateId)) {
			continue;
		}
		components[disjointSet.find(makeGateNode(gateId))].gates.push_back(gateId);
	}
	for (gate_group_id_t groupId : touchedGroups) {
		components[disjointSet.find(makeGroupNode(groupId))].groups.push_back(groupId);
	}

	for (auto& [root, component] : components) {
		if (component.groups.empty()) {
			gate_group_id_t groupId = groupIdProvider.getNewId();
			std::vector<SimulatorGate> groupedGates;
			groupedGates.reserve(component.gates.size());
			for (eval_gate_id gateId : component.gates) {
				gateIdToGroupId[gateId] = groupId;
				groupedGates.push_back(gates.at(gateId));
				ungroupedGates.erase(gateId);
			}
			compiledGroups.emplace(groupId, CompiledGateGroup(std::move(groupedGates), {}));
			dirtyGroups.insert(groupId);
			continue;
		}

		gate_group_id_t targetGroupId = component.groups.front();
		for (gate_group_id_t groupId : component.groups) {
			if (compiledGroups.at(groupId).gates.size() > compiledGroups.at(targetGroupId).gates.size()) {
				targetGroupId = groupId;
			}
		}

		for (gate_group_id_t groupId : component.groups) {
			if (groupId != targetGroupId) {
				mergeGroups(targetGroupId, groupId);
			}
		}
		compiledGroups.at(targetGroupId).gates.reserve(compiledGroups.at(targetGroupId).gates.size() + component.gates.size());
		for (eval_gate_id gateId : component.gates) {
			addGateToGroup(gateId, targetGroupId);
		}
		dirtyGroups.insert(targetGroupId);
	}
}

void LogicSimulator::refreshDirtyGroups() {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::vector<gate_group_id_t> groupsToRefresh(dirtyGroups.begin(), dirtyGroups.end());
	for (gate_group_id_t groupId : groupsToRefresh) {
		if (compiledGroups.contains(groupId)) {
			refreshGroup(groupId);
		}
	}
}

void LogicSimulator::refreshGroup(gate_group_id_t groupId) {
	CompiledGateGroup& group = compiledGroups.at(groupId);
	std::vector<SimulatorGate> refreshedGates;
	std::vector<EvalConnectionPoint> pullConnectionPoints;
	std::unordered_set<EvalConnectionPoint> pullConnectionPointSet;

	refreshedGates.reserve(group.gates.size());
	for (const SimulatorGate& oldGate : group.gates) {
		auto gateIter = gates.find(oldGate.id);
		if (gateIter == gates.end()) {
			continue;
		}
		const SimulatorGate& gate = gateIter->second;
		refreshedGates.push_back(gate);

		for (const auto& [connectionEndId, connectionsMap] : gate.connections) {
			for (const auto& [otherConnectionPoint, weight] : connectionsMap) {
				if (weight == 0) {
					continue;
				}
				if (gate.getDirection(connectionEndId, otherConnectionPoint) != InputOutput::INPUT) {
					continue;
				}
				auto otherGroupIter = gateIdToGroupId.find(otherConnectionPoint.gateId);
				if (otherGroupIter == gateIdToGroupId.end() || otherGroupIter->second == groupId) {
					continue;
				}
				if (pullConnectionPointSet.insert(otherConnectionPoint).second) {
					pullConnectionPoints.push_back(otherConnectionPoint);
				}
			}
		}
	}

	group.gates = std::move(refreshedGates);
	group.pullConnectionPoints = std::move(pullConnectionPoints);
}

std::unordered_map<gate_group_id_t, CompiledGateGroup> LogicSimulator::compileGroups() const {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::unordered_map<gate_group_id_t, CompiledGateGroup> compiledGroups;
	IdProvider<gate_group_id_t> newGroupIdProvider { 0 };
	std::unordered_set<eval_gate_id> ungroupedGates = {};
	std::unordered_set<eval_gate_id> ungroupedJunctions = {};
	std::unordered_set<eval_gate_id> groupedGateIds = {};
	for (const auto& [gateId, simulatorGate] : gates) {
		if (isJunction(gateId)) {
			ungroupedJunctions.insert(gateId);
		} else {
			ungroupedGates.insert(gateId);
		}
	}
	while (ungroupedGates.size() != 0) {
		// logInfo("{} ungrouped gates remaining", "LogicSimulator::compileGroups", ungroupedGates.size());
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
						if (!ungroupedJunctions.contains(otherGateId)) continue; // already assigned to a different group
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
							if (!ungroupedGates.contains(junctionOtherGateId)) continue; // already assigned to a different group
							groupGateIds.insert(junctionOtherGateId);
							ungroupedGates.erase(junctionOtherGateId);
							toVisit.push_back(junctionOtherGateId);
						}
					} else {
						if (!ungroupedGates.contains(otherGateId)) continue; // already assigned to a different group
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
		// logInfo("----------------------------------------------------------", "LogicSimulator::compileGroups");
		// logInfo("Group {}", "LogicSimulator::compileGroups", gateGroupId);
		// logInfo("Group gates: {}", "LogicSimulator::compileGroups", to_string(groupGateIds));
		// logInfo("Visited output connection points: {}", "LogicSimulator::compileGroups", to_string(visitedOutputConnectionPoints));
		for (eval_gate_id groupGateId : groupGateIds) {
			assert(groupedGateIds.insert(groupGateId).second && "Gate was assigned to multiple compiled groups");
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
			assert(groupedGateIds.insert(junctionGateId).second && "Junction was assigned to multiple compiled groups");
			junctionGates.push_back(gates.at(junctionGateId));
		}
		CompiledGateGroup junctionGroup(junctionGates, {}); // no pull connection points because these junctions are only going to be computed on getState;
		compiledGroups[newGroupIdProvider.getNewId()] = junctionGroup;
	}
	return compiledGroups;
}
