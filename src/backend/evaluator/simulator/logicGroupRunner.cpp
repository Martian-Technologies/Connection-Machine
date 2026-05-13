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

LogicGroupRunner::LogicGroupRunner() {
	calculateAllGateStates();
	simulationThread = std::thread(&LogicGroupRunner::simulationLoop, this);
}

LogicGroupRunner::~LogicGroupRunner() {
	{
		std::lock_guard<std::mutex> lock(controlMutex);
		simulationThreadRunning.store(false, std::memory_order_release);
	}
	controlCv.notify_all();
	if (simulationThread.joinable()) {
		simulationThread.join();
	}
}

logic_state_t LogicGroupRunner::getState(simulator_state_reference simulatorStateIndex) const {
	return statesOutputVector.at(simulatorStateIndex.get());
}

void LogicGroupRunner::setState_noCalculate(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	EvalConnectionPoint connectionPoint = simulatorStateIndexToConnectionPoint.at(simulatorStateIndex);
	if (!gateIdToGroupId.contains(connectionPoint.gateId)) {
		logError("Trying to set state for connection point {}, but its gate {} is not in any group", "LogicGroupRunner::setState", connectionPoint.toString(), connectionPoint.gateId.get());
		return;
	}
	gate_group_id_t groupId = gateIdToGroupId.at(connectionPoint.gateId);
	RunnableGateGroup& group = runnableGroups[groupId.get()];
	group.setState(connectionPoint, state);
}

void LogicGroupRunner::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	setState_noCalculate(simulatorStateIndex, state);
	calculateAllGateStates();
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
	if (newIndex.get() >= statesOutputVector.size()) {
		statesOutputVector.resize(std::bit_ceil(newIndex.get() + 1), logic_state_t::UNDEFINED);
	}
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
		gateIdToGroupId.erase(deletedGateId);
	}

	std::vector<gate_group_id_t> groupIdsToPreserve;
	for (const auto& [groupId, simGroup] : simGroups) {
		if (groupId.get() >= groupsCache.size() || groupsCache[groupId.get()] != simGroup) {
			groupIdsToPreserve.push_back(groupId);
		}
	}
	for (gate_group_id_t groupId : range(gate_group_id_t(0), gate_group_id_t(runnableGroups.size()))) {
		if (runnableGroups[groupId.get()].isEmpty()) {
			continue;
		}
		if (simGroups.find(groupId) == simGroups.end()) {
			groupIdsToPreserve.push_back(groupId);
		}
	}

	preserveStates(statesToPreserve, groupIdsToPreserve, deletedGates);

	std::vector<gate_group_id_t> groupIdsToUpdate;
	for (const auto& [groupId, simGroup] : simGroups) {
		if (setGroup(groupId, simGroup)) {
			groupIdsToUpdate.push_back(groupId);
		}
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
		setState_noCalculate(simulatorStateIndex, state);
	}

	for (gate_group_id_t groupId : groupIdsToUpdate) {
		runnableGroups[groupId.get()].runPull(*this);
	}
	for (gate_group_id_t groupId : groupIdsToUpdate) {
		runnableGroups[groupId.get()].calculateAllGateStates(*this, statesOutputVector);
	}

	resetReplay();
}

void LogicGroupRunner::preserveStates(
	std::unordered_map<EvalConnectionPoint, logic_state_t>& statesToPreserve,
	const std::vector<gate_group_id_t>& groupIdsToPreserve,
	const std::unordered_set<eval_gate_id>& deletedGates
) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	for (gate_group_id_t groupId : groupIdsToPreserve) {
		if (groupId.get() >= groupsCache.size()) {
			continue;
		}
		const LinkedGateGroup& oldSimGroup = groupsCache[groupId.get()];
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

bool LogicGroupRunner::setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (groupId.get() < groupsCache.size()) {
#ifdef TRACY_PROFILER
		ZoneScopedN("group cache check");
#endif
		if (groupsCache[groupId.get()] == simGroup) {
			return false;
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
	runnableGroups[groupId.get()] = RunnableGateGroup(simGroup, groupId, *this);
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
	return true;
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

RunnableGateGroup::RunnableGateGroup(
	const LinkedGateGroup& linkedGateGroup,
	gate_group_id_t groupId,
	LogicGroupRunner& logicGroupRunner
) : groupId(groupId), empty(false) {
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

	calculateAllGateStatesBytecode.push_back(0); // num gates, will be filled in later

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
			calculateAllGateStatesBytecode[0]++;
			calculateAllGateStatesBytecode.push_back(static_cast<unsigned int>(instructionType));
			calculateAllGateStatesBytecode.push_back(logicGroupRunner.getSimulatorStateIndex_mut(EvalConnectionPoint { gate.id, connection_end_id_t(0) }).get());
			getStateStaticInstructions[EvalConnectionPoint { gate.id, connection_end_id_t(0) }] = { instructionType, 0 };
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
			calculateAllGateStatesBytecode[0]++;
			calculateAllGateStatesBytecode.push_back(static_cast<unsigned int>(instructionType));
			unsigned int numInputsIndexForAllStates = calculateAllGateStatesBytecode.size();
			calculateAllGateStatesBytecode.push_back(0); // num inputs, will be filled in later
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
				calculateAllGateStatesBytecode[numInputsIndexForAllStates]++;
				if (allocEvalConnectionPointsMain.contains(connectionPoint)) {
					calculateAllGateStatesBytecode.push_back(0);
					calculateAllGateStatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
				} else {
					calculateAllGateStatesBytecode.push_back(1);
					calculateAllGateStatesBytecode.push_back(connectionPoint.gateId.get());
					calculateAllGateStatesBytecode.push_back(connectionPoint.connectionEndId.get());
				}
			}
			if (hasOutput){
				calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
			}
			calculateAllGateStatesBytecode.push_back(logicGroupRunner.getSimulatorStateIndex_mut(EvalConnectionPoint { gate.id, connection_end_id_t(0) }).get());
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
			// fetchInstructionsForConnectionPoint[connectionPoint] = { static_cast<unsigned int>(InstructionType::COPY), allocEvalConnectionPointsMain.getIndex(connectionPoint) };
			calculateAllGateStatesBytecode[0]++;
			calculateAllGateStatesBytecode.push_back(static_cast<unsigned int>(InstructionType::COPY));
			calculateAllGateStatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
			calculateAllGateStatesBytecode.push_back(logicGroupRunner.getSimulatorStateIndex_mut(connectionPoint).get());
			getStateStaticInstructions[connectionPoint] = { InstructionType::COPY, allocEvalConnectionPointsMain.getIndex(connectionPoint) };
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

logic_state_t RunnableGateGroup::getStaticState(EvalConnectionPoint connectionPoint) const {
	assert(!empty && "Trying to get static state from an empty group");
	const std::pair<InstructionType, unsigned int> instructionsAndDataFieldIndex = getStateStaticInstructions.at(connectionPoint);
	if (instructionsAndDataFieldIndex.first == InstructionType::SET_L) {
		return logic_state_t::LOW;
	} else if (instructionsAndDataFieldIndex.first == InstructionType::SET_H) {
		return logic_state_t::HIGH;
	} else if (instructionsAndDataFieldIndex.first == InstructionType::SET_X) {
		return logic_state_t::UNDEFINED;
	} else if (instructionsAndDataFieldIndex.first == InstructionType::SET_Z) {
		return logic_state_t::FLOATING;
	} else if (instructionsAndDataFieldIndex.first == InstructionType::COPY) {
		return dataField[instructionsAndDataFieldIndex.second];
	} else {
		assert(false && "Unknown instruction type for static state");
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

void RunnableGateGroup::calculateAllGateStates(const LogicGroupRunner& runner, std::vector<logic_state_t>& outputVector) const {
	if (empty) {
		return;
	}
	unsigned int bytecodeIndex = 0;
	unsigned int numGates = calculateAllGateStatesBytecode[bytecodeIndex++];
	for (unsigned int i = 0; i < numGates; i++) {
		InstructionType instruction = static_cast<InstructionType>(calculateAllGateStatesBytecode[bytecodeIndex++]);
		if (instruction == InstructionType::COPY) {
			unsigned int sourceIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			unsigned int destIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[destIndex] = dataField[sourceIndex];
		} else if (instruction == InstructionType::SET_L) {
			unsigned int outputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[outputIndex] = logic_state_t::LOW;
		} else if (instruction == InstructionType::SET_H) {
			unsigned int outputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[outputIndex] = logic_state_t::HIGH;
		} else if (instruction == InstructionType::SET_X) {
			unsigned int outputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[outputIndex] = logic_state_t::UNDEFINED;
		} else if (instruction == InstructionType::SET_Z) {
			unsigned int outputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[outputIndex] = logic_state_t::FLOATING;
		} else if (
			instruction == InstructionType::JUNCTION_PULL_L ||
			instruction == InstructionType::JUNCTION_PULL_H ||
			instruction == InstructionType::JUNCTION_PULL_Z ||
			instruction == InstructionType::JUNCTION_PULL_X
		) {
			unsigned int numInputs = calculateAllGateStatesBytecode[bytecodeIndex++];
			assert(numInputs >= 1 && "JUNCTION should have at least one input");
			logic_state_t accumulator = logic_state_t::FLOATING;
			bool continueProcessing = true;
			for (unsigned int j = 0; j < numInputs; j++) {
				unsigned int inputType = calculateAllGateStatesBytecode[bytecodeIndex++];
				if (continueProcessing) {
					logic_state_t inputState;
					if (inputType == 0) {
						unsigned int inputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
						inputState = dataField[inputIndex];
					} else {
						unsigned int gateId = calculateAllGateStatesBytecode[bytecodeIndex++];
						unsigned int connectionEndId = calculateAllGateStatesBytecode[bytecodeIndex++];
						EvalConnectionPoint connectionPoint { eval_gate_id(gateId), connection_end_id_t(connectionEndId) };
						inputState = runner.getStaticState(connectionPoint);
					}
					// unsigned int inputIndex = calculateAllGateStatesBytecode[bytecodeIndex+j];
					// logic_state_t inputState = dataField[inputIndex];
					// continueProcessing = ProcessingTable::junction(accumulator, inputState);
					continueProcessing = ProcessingTable::junction(accumulator, inputState);
				} else {
					if (inputType == 0) {
						bytecodeIndex++;
					} else {
						bytecodeIndex += 2;
					}
				}
			}
			if (instruction == InstructionType::JUNCTION_PULL_H) {
				accumulator = ProcessingTable::pull_up(accumulator);
			} else if (instruction == InstructionType::JUNCTION_PULL_L) {
				accumulator = ProcessingTable::pull_down(accumulator);
			} else if (instruction == InstructionType::JUNCTION_PULL_X) {
				accumulator = ProcessingTable::pull_x(accumulator);
			}
			unsigned int outputIndex = calculateAllGateStatesBytecode[bytecodeIndex++];
			outputVector[outputIndex] = accumulator;
		} else {
			assert(false && "Unsupported instruction in calculateAllGateStates");
		}
	}
};

void LogicGroupRunner::resetReplay() {
	// this gets called when the circuit gets edited and the replay keyframes are no longer valid
	{
		std::unique_lock replayLock(replayKeyframesMutex);
		replayKeyframes.clear();
	}
	saveReplayKeyframe();
}

void LogicGroupRunner::saveReplayKeyframe() {
	std::shared_lock statesLock(statesOutputVectorMutex);
	std::unique_lock replayLock(replayKeyframesMutex);
	replayKeyframes.push_back({ simulationTickIndex.load(std::memory_order_acquire), statesOutputVector });
	while (replayKeyframes.size() > maxReplayKeyframes) {
		replayKeyframes.pop_front();
	}
}

void LogicGroupRunner::calculateAllGateStates() {
	{
		std::unique_lock lock(statesOutputVectorMutex);
		for (RunnableGateGroup& group : runnableGroups) {
			group.calculateAllGateStates(*this, statesOutputVector);
		}
	}
	saveReplayKeyframe();
}

void LogicGroupRunner::tick() {
	{
#ifdef TRACY_PROFILER
		ZoneScopedN("LogicGroupRunner::tick - pull phase");
#endif
		for (RunnableGateGroup& group : runnableGroups) {
			group.runPull(*this);
		}
	}
	{
#ifdef TRACY_PROFILER
		ZoneScopedN("LogicGroupRunner::tick - tick phase");
#endif
		for (RunnableGateGroup& group : runnableGroups) {
			group.runTick();
		}
	}
	simulationTickIndex.fetch_add(1, std::memory_order_acq_rel);
	if (updateStatesOutputVectorNextUpdate.load(std::memory_order_acquire)) {
#ifdef TRACY_PROFILER
		ZoneScopedN("LogicGroupRunner::tick - calculateAllGateStates");
#endif
		calculateAllGateStates();
		updateStatesOutputVectorNextUpdate.store(false, std::memory_order_release);
	}
}

void LogicGroupRunner::simulationLoop() {
	using clock = std::chrono::steady_clock;

	auto nextTick = clock::now();
	auto lastTickTime = clock::now();
	bool isFirstTick = true;

	while (simulationThreadRunning.load(std::memory_order_acquire)) {
		bool didSprint = false;
		while (
			simulationThreadRunning.load(std::memory_order_acquire) &&
			sprintCounter.load(std::memory_order_acquire) > 0
		) {
			didSprint = true;
			auto currentTime = clock::now();
			{
				EditingGuard editingGuard = getEditingGuard();
				tick();
			}
			sprintCounter.fetch_sub(1, std::memory_order_acq_rel);
			updateEmaTickrate(currentTime, lastTickTime, isFirstTick);
			controlCv.notify_all();
		}

		if (didSprint) {
			calculateAllGateStates();
			continue;
		}

		if (running.load(std::memory_order_acquire)) {
			auto currentTime = clock::now();
			{
				EditingGuard editingGuard = getEditingGuard();
				tick();
			}
			updateEmaTickrate(currentTime, lastTickTime, isFirstTick);

			bool sleptForTickrateLimit = false;
			if (useTickrateLimiter.load(std::memory_order_acquire)) {
				double currentTargetTickrate = targetTickrate.load(std::memory_order_acquire);
				if (currentTargetTickrate > 0.0) {
					auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
						std::chrono::duration<double>(1.0 / currentTargetTickrate)
					);
					nextTick += period;
					auto waitStart = clock::now();
					if (nextTick > waitStart) {
						std::unique_lock<std::mutex> lock(controlMutex);
						controlCv.wait_until(lock, nextTick, [&] {
							return
								!simulationThreadRunning.load(std::memory_order_acquire) ||
								!running.load(std::memory_order_acquire) ||
								sprintCounter.load(std::memory_order_acquire) > 0;
						});
						sleptForTickrateLimit = true;
					}
				}
			}
			if (!sleptForTickrateLimit) {
				// When we can run flat-out, yield between exclusive lock acquisitions so readers
				// and edit operations are not starved by the simulation thread.
				std::this_thread::yield();
			}
		} else {
			calculateAllGateStates();
			averageTickrate.store(0.0, std::memory_order_release);
			std::unique_lock<std::mutex> lock(controlMutex);
			controlCv.wait(lock, [&] {
				return
					!simulationThreadRunning.load(std::memory_order_acquire) ||
					running.load(std::memory_order_acquire) ||
					sprintCounter.load(std::memory_order_acquire) > 0;
			});
			nextTick = clock::now();
			lastTickTime = nextTick;
			isFirstTick = true;
		}
	}
}

void LogicGroupRunner::updateEmaTickrate(
	const std::chrono::steady_clock::time_point& currentTime,
	std::chrono::steady_clock::time_point& lastTickTime,
	bool& isFirstTick
) {
	if (!isFirstTick) {
		auto deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTickTime);
		if (deltaTime.count() > 0) {
			double currentTickrate = 1.0e9 / static_cast<double>(deltaTime.count());
			double dtSeconds = std::chrono::duration<double>(deltaTime).count();
			double alpha = 1.0 - std::exp(-dtSeconds * std::log(2.0) / tickrateHalflife);
			double currentEMA = averageTickrate.load(std::memory_order_acquire);
			double newEMA = alpha * currentTickrate + (1.0 - alpha) * currentEMA;
			averageTickrate.store(newEMA, std::memory_order_release);
		}
	} else {
		isFirstTick = false;
	}
	lastTickTime = currentTime;
}

void LogicGroupRunner::setRunning(bool shouldRun) {
	running.store(shouldRun, std::memory_order_release);
	controlCv.notify_all();
}
void LogicGroupRunner::setRealistic(bool isRealistic) {
	realistic.store(isRealistic, std::memory_order_release);
}
void LogicGroupRunner::setUseTickrateLimiter(bool shouldUseTickrateLimiter) {
	useTickrateLimiter.store(shouldUseTickrateLimiter, std::memory_order_release);
	controlCv.notify_all();
}
void LogicGroupRunner::setTargetTickrate(double tickrate) {
	targetTickrate.store(tickrate, std::memory_order_release);
	controlCv.notify_all();
}
void LogicGroupRunner::addSprint(unsigned int nTicks) {
	sprintCounter.fetch_add(nTicks, std::memory_order_acq_rel);
	controlCv.notify_all();
}
void LogicGroupRunner::waitForSprintComplete() {
	std::unique_lock<std::mutex> lock(controlMutex);
	controlCv.wait(lock, [&] {
		return sprintCounter.load(std::memory_order_acquire) == 0;
	});
}

bool LogicGroupRunner::isRunning() const { return running.load(std::memory_order_acquire); }
bool LogicGroupRunner::isRealistic() const { return realistic.load(std::memory_order_acquire); }
bool LogicGroupRunner::getUseTickrateLimiter() const { return useTickrateLimiter.load(std::memory_order_acquire); }
double LogicGroupRunner::getTargetTickrate() const { return targetTickrate.load(std::memory_order_acquire); }
double LogicGroupRunner::getAverageTickrate() const {
	if (!isRunning()) {
		return 0.0;
	}
	double currentAverageTickrate = averageTickrate.load(std::memory_order_acquire);
	double currentTargetTickrate = getTargetTickrate();
	if (currentTargetTickrate > 0.0) {
		double percentageError = (currentAverageTickrate - currentTargetTickrate) / currentTargetTickrate;
		if (std::abs(percentageError) < 0.01) {
			return currentTargetTickrate;
		}
	}
	return currentAverageTickrate;
}
unsigned int LogicGroupRunner::getSprintCount() const { return sprintCounter.load(std::memory_order_acquire); }

bool LogicGroupRunner::stepBack() const { return false; }
bool LogicGroupRunner::stepForward() const { return false; }
bool LogicGroupRunner::skipBack() const { return false; }
bool LogicGroupRunner::skipForward() const { return false; }
bool LogicGroupRunner::isViewingReplay() const { return false; }
