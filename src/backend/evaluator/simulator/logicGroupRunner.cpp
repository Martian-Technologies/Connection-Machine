#include "logicGroupRunner.h"
#include "gateGroup.h"
#include "simBlockData.h"
#include "util/algorithm.h"
#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

using namespace SimBlockData;

namespace {
	void eraseGateMappings(
		std::unordered_map<eval_gate_id, gate_group_id_t>& gateIdToGroupId,
		const LinkedGateGroup& simGroup
	) {
		for (const SimulatorGate& gate : simGroup.gates) {
			gateIdToGroupId.erase(gate.id);
		}
	}
}

logic_state_t LogicGroupRunner::getState(simulator_state_reference simulatorStateIndex) const {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	EvalConnectionPoint connectionPoint = simulatorStateIndexToConnectionPoint.at(simulatorStateIndex);
	if (!gateIdToGroupId.contains(connectionPoint.gateId)) {
		return logic_state_t::UNDEFINED;
	}
	gate_group_id_t groupId = gateIdToGroupId.at(connectionPoint.gateId);
	const RunnableGateGroup& group = runnableGroups[groupId.get()];
	if (!groupsPulledValid) {
		if (groupsPulled.size() < runnableGroups.size()) {
			groupsPulled.resize(runnableGroups.size());
		}
		for (size_t i = 0; i < runnableGroups.size(); i++) {
			groupsPulled[i] = 0;
		}
		groupsPulledValid = true;
	}
	if (!groupsPulled[groupId.get()]) {
		group.runPull(*this);
		groupsPulled[groupId.get()] = 1;
	}
	return group.getState(*this, connectionPoint);
}

void LogicGroupRunner::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	EvalConnectionPoint connectionPoint = simulatorStateIndexToConnectionPoint.at(simulatorStateIndex);
	if (!gateIdToGroupId.contains(connectionPoint.gateId)) {
		logError("Trying to set state for connection point {}, but its gate {} is not in any group", "LogicGroupRunner::setState", connectionPoint.toString(), connectionPoint.gateId.get());
		return;
	}
	groupsPulledValid = false;
	gate_group_id_t groupId = gateIdToGroupId.at(connectionPoint.gateId);
	RunnableGateGroup& group = runnableGroups[groupId.get()];
	group.setState(connectionPoint, state);
}

simulator_state_reference LogicGroupRunner::getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const {
	if (!connectionPointToSimulatorStateIndex.contains(evalConnectionPoint)) {
		return simulator_state_reference(3); // UNDEFINED
	}
	return connectionPointToSimulatorStateIndex.at(evalConnectionPoint);
}

simulator_state_reference LogicGroupRunner::getSimulatorStateIndex_mut(EvalConnectionPoint evalConnectionPoint) {
	if (connectionPointToSimulatorStateIndex.contains(evalConnectionPoint)) {
		return connectionPointToSimulatorStateIndex[evalConnectionPoint];
	}
	simulator_state_reference newIndex = stateIndexProvider.getNewId();
	connectionPointToSimulatorStateIndex[evalConnectionPoint] = newIndex;
	simulatorStateIndexToConnectionPoint[newIndex] = evalConnectionPoint;
	return newIndex;
}

void LogicGroupRunner::setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups, const std::unordered_set<eval_gate_id>& deletedGates) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::unordered_map<EvalConnectionPoint, logic_state_t> statesToPreserve;

	for (eval_gate_id deletedGateId : deletedGates) {
		assert(gateIdToGroupId.contains(deletedGateId) && "Deleted gate should exist in gateIdToGroupId mapping");
		gateIdToGroupId.erase(deletedGateId);
	}

	preserveStates(statesToPreserve, deletedGates);

	for (const auto& [groupId, simGroup] : simGroups) {
		setGroup(groupId, simGroup);
	}

	// logInfo("Updated logic groups. Preserved states for " + to_string(statesToPreserve));
	// delete groups that got deleted
	for (gate_group_id_t groupId : range(gate_group_id_t(0), gate_group_id_t(runnableGroups.size()))) {
		if (runnableGroups[groupId.get()].isEmpty()) {
			continue;
		}
		if (simGroups.find(groupId) == simGroups.end()) {
			runnableGroups[groupId.get()] = RunnableGateGroup();
			groupsCache[groupId.get()] = LinkedGateGroup();
		}
	}
	for (const auto& [connectionPoint, state] : statesToPreserve) {
		simulator_state_reference simulatorStateIndex = getSimulatorStateIndex(connectionPoint);
		if (simulatorStateIndex.get() < 4) {
			continue; // skip constants
		}
		setState(simulatorStateIndex, state);
	}
}

void LogicGroupRunner::preserveStates(std::unordered_map<EvalConnectionPoint, logic_state_t>& statesToPreserve, const std::unordered_set<eval_gate_id>& deletedGates) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	for (const LinkedGateGroup& oldSimGroup : groupsCache) {
		if (oldSimGroup.gates.empty()) {
			continue;
		}
		for (const SimulatorGate& gate : oldSimGroup.gates) {
			if (deletedGates.contains(gate.id)) {
				continue; // gate got deleted, so we don't need to preserve its state
			}
			const std::vector<connection_end_id_t>& outputPorts = SimBlockData::getOutputPorts(gate.type);
			for (connection_end_id_t outputPort : outputPorts) {
				EvalConnectionPoint connectionPoint { gate.id, outputPort };
				simulator_state_reference simulatorStateIndex = getSimulatorStateIndex(connectionPoint);
				if (simulatorStateIndex.get() < 4) {
					continue; // skip constants
				}
				logic_state_t state = getState(simulatorStateIndex);
				statesToPreserve[connectionPoint] = state;
			}
		}
	}
}

void LogicGroupRunner::setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	groupsPulledValid = false;
	if (groupId.get() < groupsCache.size()) {
#ifdef TRACY_PROFILER
		ZoneScopedN("group cache check");
#endif
		if (groupsCache[groupId.get()] == simGroup) {
			return;
		}
	}
	if (groupsCache.size() <= groupId.get()) {
#ifdef TRACY_PROFILER
		ZoneScopedN("resize");
#endif
		unsigned int newSize = groupsCache.size() * 2;
		newSize = std::max(newSize, 1u);
		while (newSize <= groupId.get()) {
			newSize *= 2;
		}
		groupsCache.resize(newSize);
		runnableGroups.resize(newSize);
	}
	{
#ifdef TRACY_PROFILER
		ZoneScopedN("groupsCache[groupId.get()] = simGroup");
#endif
		groupsCache[groupId.get()] = simGroup;
	}
	runnableGroups[groupId.get()] = RunnableGateGroup(simGroup, groupId);
	for (const SimulatorGate& gate : simGroup.gates) {
#ifdef TRACY_PROFILER
		ZoneScopedN("setGroup gateIdToGroupId");
#endif
		gateIdToGroupId[gate.id] = groupId;
		const std::vector<connection_end_id_t>& outputPorts = SimBlockData::getOutputPorts(gate.type);
		for (connection_end_id_t outputPort : outputPorts) {
			EvalConnectionPoint connectionPoint { gate.id, outputPort };
			getSimulatorStateIndex_mut(connectionPoint);
		}
	}
}

namespace {
	inline bool isJunction(BlockType blockType) {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_X;
	}
	inline bool isJunction(InstructionType instruction) {
		return instruction == InstructionType::JUNCTION_PULL_L || instruction == InstructionType::JUNCTION_PULL_H || instruction == InstructionType::JUNCTION_PULL_Z || instruction == InstructionType::JUNCTION_PULL_X;
	}
}

namespace {

	template <class T>
	class Indexer {
	public:
		Indexer(unsigned int& nextIndex) : nextIndex(nextIndex) { }
		unsigned int getIndex(const T& value) {
			auto it = valueToIndex.find(value);
			if (it != valueToIndex.end()) {
				return it->second;
			}
			unsigned int index = nextIndex++;
			valueToIndex[value] = index;
			return index;
		}
		unsigned int size() const {
			return valueToIndex.size();
		}
		bool contains(const T& value) const {
			return valueToIndex.contains(value);
		}
	private:
		std::unordered_map<T, unsigned int> valueToIndex;
		unsigned int& nextIndex;
	};

}

RunnableGateGroup::RunnableGateGroup(const LinkedGateGroup& linkedGateGroup, gate_group_id_t groupId) : groupId(groupId), empty(false) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	std::unordered_map<EvalConnectionPoint, unsigned int> pulledConnectionPointToDataFieldIndex;
	std::unordered_map<gate_group_id_t, std::vector<std::pair<unsigned int, EvalConnectionPoint>>> pullIndicesToPullFromGroups;
	std::vector<std::pair<unsigned int, logic_state_t>> dataVectorInitializers;
	for (const auto& [connectionPoint, groupIdAndIndex] : linkedGateGroup.pullConnectionPoints) {
		pullIndicesToPullFromGroups[groupIdAndIndex.first].push_back({ groupIdAndIndex.second, connectionPoint });
	}
	pullDataBytecode.push_back(0); // num groups
	unsigned int dataFieldAllocator = 0;
	Indexer<EvalConnectionPoint> allocEvalConnectionPointsMain(dataFieldAllocator);
	Indexer<EvalConnectionPoint> allocEvalConnectionPointsReserved(dataFieldAllocator);

	for (const auto& [groupId, pullIndicesAndConnectionPoints] : pullIndicesToPullFromGroups) {
#ifdef TRACY_PROFILER
		ZoneScopedN("pull group");
#endif
		pullDataBytecode.push_back(groupId.get()); // group id
		pullDataBytecode.push_back(pullIndicesAndConnectionPoints.size()); // num connection points to pull from in this group
		for (const auto& [pullIndex, connectionPoint] : pullIndicesAndConnectionPoints) {
			pullDataBytecode.push_back(pullIndex); // pull index within the group
			allocEvalConnectionPointsMain.getIndex(connectionPoint); // always +1, so we don't need to store the index for the pull phase
		}
		pullDataBytecode[0]++;
	}

	for (EvalConnectionPoint pushConnectionPoint : linkedGateGroup.pushConnectionPoints) {
		publishedStateDataFieldIndices.push_back(allocEvalConnectionPointsMain.getIndex(pushConnectionPoint));
	}

	std::unordered_map<eval_gate_id, SimulatorGate> gates;
	for (const SimulatorGate& gate : linkedGateGroup.gates) {
		gates[gate.id] = gate;
	}

	std::unordered_set<eval_gate_id> internalNonJunctionGates;
	calculateGatesBytecode.push_back(0); // num gates, will be filled in later
	std::vector<unsigned int> copyOldStatesBytecode;
	std::vector<unsigned int> simulateBytecode;
	for (const SimulatorGate& gate : linkedGateGroup.gates) { // calculate junctions
#ifdef TRACY_PROFILER
		ZoneScopedN("junction");
#endif
		if (!isJunction(gate.type)) {
			internalNonJunctionGates.insert(gate.id);
			continue;
		}
		bool hasOutput = false;
		bool hasInput = false;
		for (const auto& [connectionPoint, weight] : gate.getConnectionsFromPort(connection_end_id_t(0))) {
			InputOutput direction = gate.getDirection(connection_end_id_t(0), connectionPoint);
			if (direction == InputOutput::OUTPUT) {
				hasOutput = true;
			}
			if (direction == InputOutput::INPUT) {
				hasInput = true;
			}
			if (hasOutput && hasInput) {
				break;
			}
		}

		if (!hasInput) {
			// junctions with no inputs are treated as constants
			BlockType blockTypeEquivalent = BlockType::NONE;
			if (gate.type == BlockType::JUNCTION) {
				blockTypeEquivalent = BlockType::CONSTANT_Z;
			} else if (gate.type == BlockType::JUNCTION_H) {
				blockTypeEquivalent = BlockType::CONSTANT_ON;
			} else if (gate.type == BlockType::JUNCTION_L) {
				blockTypeEquivalent = BlockType::CONSTANT_OFF;
			} else if (gate.type == BlockType::JUNCTION_X) {
				blockTypeEquivalent = BlockType::CONSTANT_X;
			}
			assert(blockTypeEquivalent != BlockType::NONE && "Unknown junction type");
			if (hasOutput){
				// calculateGatesBytecode[0]++;
				// calculateGatesBytecode.push_back(static_cast<unsigned int>(blockTypeEquivalent)); // block type
				// calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
				logic_state_t state;
				if (blockTypeEquivalent == BlockType::CONSTANT_OFF) {
					state = logic_state_t::LOW;
				} else if (blockTypeEquivalent == BlockType::CONSTANT_ON) {
					state = logic_state_t::HIGH;
				} else if (blockTypeEquivalent == BlockType::CONSTANT_X) {
					state = logic_state_t::UNDEFINED;
				} else if (blockTypeEquivalent == BlockType::CONSTANT_Z) {
					state = logic_state_t::FLOATING;
				}
				dataVectorInitializers.push_back({ allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }), state });
			}
			InstructionType instructionType;
			if (blockTypeEquivalent == BlockType::CONSTANT_OFF) {
				instructionType = InstructionType::SET_L;
			} else if (blockTypeEquivalent == BlockType::CONSTANT_ON) {
				instructionType = InstructionType::SET_H;
			} else if (blockTypeEquivalent == BlockType::CONSTANT_X) {
				instructionType = InstructionType::SET_X;
			} else if (blockTypeEquivalent == BlockType::CONSTANT_Z) {
				instructionType = InstructionType::SET_Z;
			} else {
				assert(false && "Unknown block type equivalent for junction with no inputs");
			}
			fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }] = { static_cast<unsigned int>(instructionType) };
		} else {
			// junctions with one or more inputs
			InstructionType instructionType;
			if (gate.type == BlockType::JUNCTION) {
				instructionType = InstructionType::JUNCTION_PULL_Z;
			} else if (gate.type == BlockType::JUNCTION_H) {
				instructionType = InstructionType::JUNCTION_PULL_H;
			} else if (gate.type == BlockType::JUNCTION_L) {
				instructionType = InstructionType::JUNCTION_PULL_L;
			} else if (gate.type == BlockType::JUNCTION_X) {
				instructionType = InstructionType::JUNCTION_PULL_X;
			} else {
				assert(false && "Unknown junction type");
			}
			if (hasOutput) {
				calculateGatesBytecode[0]++;
				calculateGatesBytecode.push_back(static_cast<unsigned int>(instructionType)); // block type
			}
			fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }] = { static_cast<unsigned int>(instructionType), 0 };
			unsigned int numInputsIndex = calculateGatesBytecode.size();
			if (hasOutput) {
				calculateGatesBytecode.push_back(0); // num inputs, will be filled in later
			}
			for (const auto& [connectionPoint, weight] : gate.getConnectionsFromPort(connection_end_id_t(0))) {
				InputOutput direction = gate.getDirection(connection_end_id_t(0), connectionPoint);
				if (direction != InputOutput::INPUT) {
					continue;
				}
				if (hasOutput){
					calculateGatesBytecode[numInputsIndex]++;
					calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
				}
				fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }][1]++;
				if (allocEvalConnectionPointsMain.contains(connectionPoint)) {
					fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }].push_back(0);
					fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }].push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
				} else {
					fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }].push_back(1);
					fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }].push_back(connectionPoint.gateId.get());
					fetchInstructionsForConnectionPoint[EvalConnectionPoint { gate.id, connection_end_id_t(0) }].push_back(connectionPoint.connectionEndId.get());
				}
			}
			if (hasOutput){
				calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
			}
		}
	}
	std::unordered_set<eval_gate_id> nonJunctionGatesWhoseStateGotWritten;
	for (const SimulatorGate& gate : linkedGateGroup.gates) { // calculate non-junctions
#ifdef TRACY_PROFILER
		ZoneScopedN("gate");
#endif
		if (isJunction(gate.type)) {
			continue;
		}
		// logInfo("Processing gate {} of type {}", "RunnableGateGroup::RunnableGateGroup", gate.id, blocktype_to_string(gate.type));

		const std::vector<connection_end_id_t>& outputPorts = SimBlockData::getOutputPorts(gate.type);
		for (connection_end_id_t outputPort : outputPorts) {
			EvalConnectionPoint connectionPoint { gate.id, outputPort };
			fetchInstructionsForConnectionPoint[connectionPoint] = { static_cast<unsigned int>(InstructionType::COPY), allocEvalConnectionPointsMain.getIndex(connectionPoint) };
			dataFieldIndexForSetState[connectionPoint] = allocEvalConnectionPointsMain.getIndex(connectionPoint);
		}

		for (const auto& [connectionEndId, connectionsFromPort] : gate.connections) {
			for (const auto& [connectionPoint, weight] : connectionsFromPort) {
				if (gate.getDirection(connectionEndId, connectionPoint) == InputOutput::OUTPUT) {
					continue;
				}
				if (!internalNonJunctionGates.contains(connectionPoint.gateId)) {
					// external gates are already fixed at the pull phase
					// and if it's a junction, we *want* the new state already
					continue;
				}
				if (!nonJunctionGatesWhoseStateGotWritten.contains(connectionPoint.gateId)) {
					// for gates which will be simulated after this gate, we can still access
					// their old state directly
					continue;
				}
				if (allocEvalConnectionPointsReserved.contains(connectionPoint)) {
					continue;
				}
				// at this point, connectionPoint is an output of a gate that is internal to the group and had its state written to before we simulate this gate, which means we need to copy its state at the start of the tick before we simulate anything else
				// copyOldStatesBytecode runs after junctions, but before any non-junction gates
				// logInfo("Adding to copyOldStatesBytecode for gate {}, connection point {}", "RunnableGateGroup::RunnableGateGroup", connectionPoint.gateId, connectionPoint.connectionEndId);
				copyOldStatesBytecode.push_back(static_cast<unsigned int>(InstructionType::COPY));
				copyOldStatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
				copyOldStatesBytecode.push_back(allocEvalConnectionPointsReserved.getIndex(connectionPoint));
				calculateGatesBytecode[0]++;
			}
		}

		if (gate.type == BlockType::BUFFER || gate.type == BlockType::NOT) {
			calculateGatesBytecode[0]++;
			// check if the gate has an input
			const auto& connectionsFromPort = gate.getConnectionsFromPort(connection_end_id_t(0));
			if (connectionsFromPort.size() == 0) {
				// treat as constant X
				simulateBytecode.push_back(static_cast<unsigned int>(InstructionType::SET_X)); // block type
			} else {
				assert(connectionsFromPort.size() == 1 && "Buffer/Not gates should have at most one input");
				simulateBytecode.push_back(static_cast<unsigned int>(gate.type == BlockType::NOT ? InstructionType::NOT : InstructionType::BUFFER));
				unsigned int index = allocEvalConnectionPointsReserved.contains(connectionsFromPort.begin()->first) ? allocEvalConnectionPointsReserved.getIndex(connectionsFromPort.begin()->first) : allocEvalConnectionPointsMain.getIndex(connectionsFromPort.begin()->first);
				simulateBytecode.push_back(index);
			}
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
			nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
		} else if (
			gate.type == BlockType::AND ||
			gate.type == BlockType::OR ||
			gate.type == BlockType::XOR ||
			gate.type == BlockType::NAND ||
			gate.type == BlockType::NOR ||
			gate.type == BlockType::XNOR
		) {
			calculateGatesBytecode[0]++;
			const auto& connectionsFromPort = gate.getConnectionsFromPort(connection_end_id_t(0));
			if (connectionsFromPort.size() == 0) {
				simulateBytecode.push_back(static_cast<unsigned int>(InstructionType::SET_L)); // block type
				simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
				nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
				continue;
			}
			InstructionType instructionType;
			if (gate.type == BlockType::AND) {
				instructionType = InstructionType::AND;
			} else if (gate.type == BlockType::OR) {
				instructionType = InstructionType::OR;
			} else if (gate.type == BlockType::XOR) {
				instructionType = InstructionType::XOR;
			} else if (gate.type == BlockType::NAND) {
				instructionType = InstructionType::NAND;
			} else if (gate.type == BlockType::NOR) {
				instructionType = InstructionType::NOR;
			} else if (gate.type == BlockType::XNOR) {
				instructionType = InstructionType::XNOR;
			} else {
				assert(false && "Unknown gate type");
			}
			simulateBytecode.push_back(static_cast<unsigned int>(instructionType)); // block type
			unsigned int numInputsIndex = simulateBytecode.size();
			simulateBytecode.push_back(0); // num inputs, will be filled in later
			for (const auto& [connectionPoint, weight] : connectionsFromPort) {
				InputOutput direction = gate.getDirection(connection_end_id_t(0), connectionPoint);
				if (direction != InputOutput::INPUT || weight == 0) {
					continue;
				}
				unsigned int index = allocEvalConnectionPointsReserved.contains(connectionPoint) ? allocEvalConnectionPointsReserved.getIndex(connectionPoint) : allocEvalConnectionPointsMain.getIndex(connectionPoint);
				for (unsigned int i = 0; i < weight; i++) {
					simulateBytecode[numInputsIndex]++;
					simulateBytecode.push_back(index);
				}
			}
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
			nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
		} else if (
			gate.type == BlockType::CONSTANT_OFF ||
			gate.type == BlockType::CONSTANT_ON ||
			gate.type == BlockType::CONSTANT_X ||
			gate.type == BlockType::CONSTANT_Z
		) {
			dataFieldIndexForSetState.erase(EvalConnectionPoint { gate.id, connection_end_id_t(0) });
			// simulateBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
			// simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
			logic_state_t defaultState;
			if (gate.type == BlockType::CONSTANT_OFF) {
				defaultState = logic_state_t::LOW;
			} else if (gate.type == BlockType::CONSTANT_ON) {
				defaultState = logic_state_t::HIGH;
			} else if (gate.type == BlockType::CONSTANT_X) {
				defaultState = logic_state_t::UNDEFINED;
			} else if (gate.type == BlockType::CONSTANT_Z) {
				defaultState = logic_state_t::FLOATING;
			}
			dataVectorInitializers.push_back({
				allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }),
				defaultState
			});
			continue;
		} else if (gate.type == BlockType::BUTTON || gate.type == BlockType::SWITCH) {
			allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }); // allocate data field index for the button/switch state, even though it won't be used in simulateBytecode because buttons/switches are controlled externally, not simulated
		} else if (gate.type == BlockType::TICK_BUTTON) {
			calculateGatesBytecode[0]++;
			simulateBytecode.push_back(static_cast<unsigned int>(InstructionType::SET_L)); // block type
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
		} else if (gate.type == BlockType::TRISTATE_BUFFER) {
			calculateGatesBytecode[0]++;
			// 0 - data
			// 1 - control
			// 2 - output

			const auto& dataConnections = gate.getConnectionsFromPort(connection_end_id_t(0));
			assert(dataConnections.size() <= 1 && "Tristate buffer should have exactly one data input");
			const auto& controlConnections = gate.getConnectionsFromPort(connection_end_id_t(1));
			assert(controlConnections.size() <= 1 && "Tristate buffer should have at most one control input");
			if (controlConnections.size() == 0) {
				// treat as const X
				simulateBytecode.push_back(static_cast<unsigned int>(InstructionType::SET_X)); // control is X
				simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(2) }));
				dataVectorInitializers.push_back({ allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(2) }), logic_state_t::UNDEFINED });
				continue;
			}
			unsigned int controlIndex = allocEvalConnectionPointsReserved.contains(controlConnections.begin()->first) ? allocEvalConnectionPointsReserved.getIndex(controlConnections.begin()->first) : allocEvalConnectionPointsMain.getIndex(controlConnections.begin()->first);
			unsigned int dataIndex;
			if (dataConnections.size() == 0) {
				dataIndex = dataFieldAllocator++;
				dataVectorInitializers.push_back({ dataIndex, logic_state_t::FLOATING });
			} else {
				dataIndex = allocEvalConnectionPointsReserved.contains(dataConnections.begin()->first) ? allocEvalConnectionPointsReserved.getIndex(dataConnections.begin()->first) : allocEvalConnectionPointsMain.getIndex(dataConnections.begin()->first);
			}
			simulateBytecode.push_back(static_cast<unsigned int>(InstructionType::TRISTATE)); // block type
			simulateBytecode.push_back(dataIndex);
			simulateBytecode.push_back(controlIndex);
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(2) }));
			dataVectorInitializers.push_back({ allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(2) }), logic_state_t::FLOATING });

		} else {
			// assert(false && "Unsupported gate type in logic group");
			logError("Unsupported gate type {} in logic group", "RunnableGateGroup::RunnableGateGroup", blocktype_to_string(gate.type));
		}
	}

	calculateGatesBytecode.insert(calculateGatesBytecode.end(), copyOldStatesBytecode.begin(), copyOldStatesBytecode.end());
	calculateGatesBytecode.insert(calculateGatesBytecode.end(), simulateBytecode.begin(), simulateBytecode.end());

	dataField.resize(dataFieldAllocator);

	for (const auto& [dataFieldIndex, state] : dataVectorInitializers) {
		dataField[dataFieldIndex] = state;
	}

	// logInfo("Group ID: {}", "RunnableGateGroup::RunnableGateGroup", groupId);
	// logInfo("dataField size: {}", "RunnableGateGroup::RunnableGateGroup", dataField.size());
	// logInfo("pullBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(pullDataBytecode));
	// logInfo("calculateBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(calculateGatesBytecode));
	// logInfo("publishedStateDataFieldIndices: {}", "RunnableGateGroup::RunnableGateGroup", to_string(publishedStateDataFieldIndices));
}

logic_state_t RunnableGateGroup::getState(const LogicGroupRunner& runner, EvalConnectionPoint connectionPoint) const {
	if (empty) {
		return logic_state_t::UNDEFINED;
	}
	const std::vector<unsigned int>& fetchInstructions = fetchInstructionsForConnectionPoint.at(connectionPoint);
	InstructionType instruction = static_cast<InstructionType>(fetchInstructions[0]);
	if (isJunction(instruction)) {
		unsigned int numInputs = fetchInstructions[1];
		logic_state_t result = logic_state_t::FLOATING;
		unsigned int instructionIndex = 2;
		for (unsigned int i = 0; i < numInputs; i++) {
			unsigned int fetchType = fetchInstructions[instructionIndex++];
			logic_state_t inputState;
			if (fetchType == 0) {
				unsigned int index = fetchInstructions[instructionIndex++];
				inputState = dataField[index];
			} else {
				unsigned int gateId = fetchInstructions[instructionIndex++];
				unsigned int connectionEndId = fetchInstructions[instructionIndex++];
				inputState = runner.getStaticState(EvalConnectionPoint { eval_gate_id(gateId), connection_end_id_t(connectionEndId) });
			}
			if (inputState == logic_state_t::UNDEFINED) {
				return logic_state_t::UNDEFINED;
			} else if (inputState == logic_state_t::HIGH) {
				if (result == logic_state_t::LOW) {
					return logic_state_t::UNDEFINED;
				}
				result = logic_state_t::HIGH;
			} else if (inputState == logic_state_t::LOW) {
				if (result == logic_state_t::HIGH) {
					return logic_state_t::UNDEFINED;
				}
				result = logic_state_t::LOW;
			}
		}
		if (result == logic_state_t::FLOATING) {
			// if there are no inputs, the junction is treated as a constant, so we return the appropriate constant value based on the junction type
			if (instruction == InstructionType::JUNCTION_PULL_H) {
				return logic_state_t::HIGH;
			} else if (instruction == InstructionType::JUNCTION_PULL_L) {
				return logic_state_t::LOW;
			} else if (instruction == InstructionType::JUNCTION_PULL_X) {
				return logic_state_t::UNDEFINED;
			}
			return logic_state_t::FLOATING;
		}
		return result;
	} else {
		return getStaticState(connectionPoint);
	}
}

logic_state_t RunnableGateGroup::getStaticState(EvalConnectionPoint connectionPoint) const {
	assert(!empty && "getStaticState should not be called for empty groups");
	const std::vector<unsigned int>& fetchInstructions = fetchInstructionsForConnectionPoint.at(connectionPoint);
	InstructionType instruction = static_cast<InstructionType>(fetchInstructions[0]);
	if (instruction == InstructionType::COPY) {
		unsigned int index = fetchInstructions[1];
		return dataField[index];
	} else if (instruction == InstructionType::SET_L) {
		return logic_state_t::LOW;
	} else if (instruction == InstructionType::SET_H) {
		return logic_state_t::HIGH;
	} else if (instruction == InstructionType::SET_X) {
		return logic_state_t::UNDEFINED;
	} else if (instruction == InstructionType::SET_Z) {
		return logic_state_t::FLOATING;
	} else {
		assert(false && "Unsupported instruction in getStaticState");
		return logic_state_t::UNDEFINED;
	}
}

void RunnableGateGroup::runPull(const LogicGroupRunner& runner) const {
	if (empty) {
		return;
	}
	unsigned int bytecodeIndex = 0;
	unsigned int numGroups = pullDataBytecode[bytecodeIndex++];
	unsigned int cnt = 0;
	for (unsigned int i = 0; i < numGroups; i++) {
		gate_group_id_t groupId(pullDataBytecode[bytecodeIndex++]);
		const RunnableGateGroup& group = runner.getGroup(groupId);
		unsigned int numPulls = pullDataBytecode[bytecodeIndex++];
		for (unsigned int j = 0; j < numPulls; j++) {
			unsigned int pullIndex = pullDataBytecode[bytecodeIndex++];
			logic_state_t pulledState = group.getState(pullIndex);
			dataField[cnt++] = pulledState;
		}
	}
}

logic_state_t LogicGroupRunner::getStaticState(EvalConnectionPoint connectionPoint) const {
	if (!gateIdToGroupId.contains(connectionPoint.gateId)) {
		return logic_state_t::UNDEFINED;
	}
	gate_group_id_t groupId = gateIdToGroupId.at(connectionPoint.gateId);
	const RunnableGateGroup& group = runnableGroups[groupId.get()];
	return group.getStaticState(connectionPoint);
}
void RunnableGateGroup::runTick() {
	if (empty) {
		return;
	}
	unsigned int bytecodeIndex = 0;
	unsigned int numGates = calculateGatesBytecode[bytecodeIndex++];
	for (unsigned int i = 0; i < numGates; i++) {
		InstructionType instruction = static_cast<InstructionType>(calculateGatesBytecode[bytecodeIndex++]);
		if (instruction == InstructionType::COPY) {
			unsigned int sourceIndex = calculateGatesBytecode[bytecodeIndex++];
			unsigned int destIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[destIndex] = dataField[sourceIndex];
		} else if (instruction == InstructionType::BUFFER) {
			unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex++];
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = ProcessingTable::buffer(dataField[inputIndex]);
		} else if (instruction == InstructionType::NOT) {
			unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex++];
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = ProcessingTable::not_gate(dataField[inputIndex]);
		} else if (instruction == InstructionType::AND) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "AND gate should have at least one input");
			logic_state_t accumulator = logic_state_t::HIGH;
			bool continueProcessing = true;
			for (unsigned int j = 0; continueProcessing && j < numInputs; j++) {
				unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
				continueProcessing = ProcessingTable::and_gate(accumulator, dataField[inputIndex]);
			}
			bytecodeIndex += numInputs;
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::NAND) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "NAND gate should have at least one input");
			logic_state_t accumulator = logic_state_t::HIGH;
			bool continueProcessing = true;
			for (unsigned int j = 0; continueProcessing && j < numInputs; j++) {
				unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
				continueProcessing = ProcessingTable::and_gate(accumulator, dataField[inputIndex]);
			}
			bytecodeIndex += numInputs;
			ProcessingTable::invert_no_float_safe(accumulator);
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::OR) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "OR gate should have at least one input");
			logic_state_t accumulator = logic_state_t::LOW;
			bool continueProcessing = true;
			for (unsigned int j = 0; continueProcessing && j < numInputs; j++) {
				unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
				continueProcessing = ProcessingTable::or_gate(accumulator, dataField[inputIndex]);
			}
			bytecodeIndex += numInputs;
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::NOR) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "NOR gate should have at least one input");
			logic_state_t accumulator = logic_state_t::LOW;
			bool continueProcessing = true;
			for (unsigned int j = 0; continueProcessing && j < numInputs; j++) {
				unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
				continueProcessing = ProcessingTable::or_gate(accumulator, dataField[inputIndex]);
			}
			bytecodeIndex += numInputs;
			ProcessingTable::invert_no_float_safe(accumulator);
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::XOR) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "XOR gate should have at least one input");
			logic_state_t accumulator = ProcessingTable::buffer(dataField[calculateGatesBytecode[bytecodeIndex]]);
			if (accumulator != logic_state_t::UNDEFINED) {
				bool continueProcessing = true;
				for (unsigned int j = 1; continueProcessing && j < numInputs; j++) {
					unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
					continueProcessing = ProcessingTable::xor_gate(accumulator, dataField[inputIndex]);
				}
			}
			bytecodeIndex += numInputs;
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::XNOR) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "XNOR gate should have at least one input");
			logic_state_t accumulator = ProcessingTable::not_gate(dataField[calculateGatesBytecode[bytecodeIndex]]);
			if (accumulator != logic_state_t::UNDEFINED) {
				bool continueProcessing = true;
				for (unsigned int j = 1; continueProcessing && j < numInputs; j++) {
					unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex + j];
					continueProcessing = ProcessingTable::xor_gate(accumulator, dataField[inputIndex]);
				}
			}
			bytecodeIndex += numInputs;
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::SET_L) {
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = logic_state_t::LOW;
		} else if (instruction == InstructionType::SET_H) {
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = logic_state_t::HIGH;
		} else if (instruction == InstructionType::SET_X) {
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = logic_state_t::UNDEFINED;
		} else if (instruction == InstructionType::SET_Z) {
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = logic_state_t::FLOATING;
		} else if (isJunction(instruction)) {
			unsigned int numInputs = calculateGatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "JUNCTION should have at least one input");
			logic_state_t accumulator = logic_state_t::FLOATING;
			bool continueProcessing = true;
			for (unsigned int j = 0; continueProcessing && j < numInputs; j++) {
				unsigned int inputIndex = calculateGatesBytecode[bytecodeIndex+j];
				logic_state_t inputState = dataField[inputIndex];
				continueProcessing = ProcessingTable::junction(accumulator, inputState);
			}
			bytecodeIndex += numInputs;
			if (instruction == InstructionType::JUNCTION_PULL_H) {
				accumulator = ProcessingTable::pull_up(accumulator);
			} else if (instruction == InstructionType::JUNCTION_PULL_L) {
				accumulator = ProcessingTable::pull_down(accumulator);
			} else if (instruction == InstructionType::JUNCTION_PULL_X) {
				accumulator = ProcessingTable::pull_x(accumulator);
			}
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = accumulator;
		} else if (instruction == InstructionType::TRISTATE) {
			unsigned int dataIndex = calculateGatesBytecode[bytecodeIndex++];
			unsigned int controlIndex = calculateGatesBytecode[bytecodeIndex++];
			unsigned int outputIndex = calculateGatesBytecode[bytecodeIndex++];
			dataField[outputIndex] = ProcessingTable::tristate(dataField[dataIndex], dataField[controlIndex]);
		} else {
			assert(false && "Unsupported block type in runTick");
		}
	}
}

void RunnableGateGroup::setState(const EvalConnectionPoint& connectionPoint, logic_state_t state) {
	if (empty) {
		assert(false && "setState should not be called for empty groups");
		return;
	}
	if (!dataFieldIndexForSetState.contains(connectionPoint)) {
		return;
	}
	unsigned int dataFieldIndex = dataFieldIndexForSetState.at(connectionPoint);
	dataField[dataFieldIndex] = state;
}


void LogicGroupRunner::tick() {
	groupsPulledValid = false;
	for (RunnableGateGroup& group : runnableGroups) {
		group.runPull(*this);
	}
	for (RunnableGateGroup& group : runnableGroups) {
		group.runTick();
	}
}

void LogicGroupRunner::setRunning(bool running) {}
void LogicGroupRunner::setRealistic(bool realistic) {}
void LogicGroupRunner::setUseTickrateLimiter(bool useTickrateLimiter) {}
void LogicGroupRunner::setTargetTickrate(double tickrate) {}
void LogicGroupRunner::addSprint(unsigned int nTicks) {
	EditingGuard editingGuard = getEditingGuard();
	for (unsigned int i = 0; i < nTicks; i++) {
		tick();
	}
}
void LogicGroupRunner::waitForSprintComplete() {}

bool LogicGroupRunner::isRunning() const { return false; }
bool LogicGroupRunner::isRealistic() const { return false; }
bool LogicGroupRunner::getUseTickrateLimiter() const { return false; }
double LogicGroupRunner::getTargetTickrate() const { return 0; }
double LogicGroupRunner::getAverageTickrate() const { return 0; }
unsigned int LogicGroupRunner::getSprintCount() const { return 0; }

bool LogicGroupRunner::stepBack() const { return false; }
bool LogicGroupRunner::stepForward() const { return false; }
bool LogicGroupRunner::skipBack() const { return false; }
bool LogicGroupRunner::skipForward() const { return false; }
bool LogicGroupRunner::isViewingReplay() const { return false; }
