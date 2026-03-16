#include "logicGroupRunner.h"
#include "gateGroup.h"
#include "simBlockData.h"
#include "util/algorithm.h"

using namespace SimBlockData;

logic_state_t LogicGroupRunner::getState(simulator_state_reference simulatorStateIndex) const {
	return logic_state_t::UNDEFINED;
}

void LogicGroupRunner::setState(simulator_state_reference simulatorStateIndex, logic_state_t state) {
	logError("setState not implemented", "LogicGroupRunner::setState");
}

void LogicGroupRunner::setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups) {
	for (const auto& [groupId, simGroup] : simGroups) {
		setGroup(groupId, simGroup);
	}
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
}

void LogicGroupRunner::setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup) {
	if (groupId.get() < groupsCache.size()) {
		if (groupsCache[groupId.get()] == simGroup) {
			return;
		}
	}
	while (groupsCache.size() <= groupId.get()) {
		groupsCache.emplace_back();
	}
	groupsCache[groupId.get()] = simGroup;

	while (runnableGroups.size() <= groupId.get()) {
		runnableGroups.emplace_back();
	}
	runnableGroups[groupId.get()] = RunnableGateGroup(simGroup, groupId);
}

namespace {
	bool isJunction(BlockType blockType) {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_X;
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

RunnableGateGroup::RunnableGateGroup(const LinkedGateGroup& linkedGateGroup, gate_group_id_t groupId) {
	empty = false;
	std::unordered_map<EvalConnectionPoint, unsigned int> pulledConnectionPointToDataFieldIndex;
	std::unordered_map<gate_group_id_t, std::vector<std::pair<unsigned int, EvalConnectionPoint>>> pullIndicesToPullFromGroups;
	for (const auto& [connectionPoint, groupIdAndIndex] : linkedGateGroup.pullConnectionPoints) {
		pullIndicesToPullFromGroups[groupIdAndIndex.first].push_back({ groupIdAndIndex.second, connectionPoint });
	}
	pullDataBytecode.push_back(0); // num groups
	unsigned int dataFieldAllocator = 0;
	Indexer<EvalConnectionPoint> allocEvalConnectionPointsMain(dataFieldAllocator);
	Indexer<EvalConnectionPoint> allocEvalConnectionPointsReserved(dataFieldAllocator);

	for (const auto& [groupId, pullIndicesAndConnectionPoints] : pullIndicesToPullFromGroups) {
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
		if (!hasOutput) {
			// no calculation needed for junctions without outputs during tick, instead they are computed on fetch
			continue;
		}
		calculateGatesBytecode[0]++;
		if (!hasInput) { // junctions with no inputs are treated as constants
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
			calculateGatesBytecode.push_back(static_cast<unsigned int>(blockTypeEquivalent)); // block type
			calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
			continue;
		}

		// junctions with one or more inputs
		calculateGatesBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
		unsigned int numInputsIndex = calculateGatesBytecode.size();
		calculateGatesBytecode.push_back(0); // num inputs, will be filled in later
		for (const auto& [connectionPoint, weight] : gate.getConnectionsFromPort(connection_end_id_t(0))) {
			InputOutput direction = gate.getDirection(connection_end_id_t(0), connectionPoint);
			if (direction != InputOutput::INPUT) {
				continue;
			}
			calculateGatesBytecode[numInputsIndex]++;
			calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint));
		}
		calculateGatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(0) }));
	}
	std::unordered_set<eval_gate_id> nonJunctionGatesWhoseStateGotWritten;
	for (const SimulatorGate& gate : linkedGateGroup.gates) { // calculate non-junctions
		if (isJunction(gate.type)) {
			continue;
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
				logInfo("Adding to copyOldStatesBytecode for gate {}, connection point {}", "RunnableGateGroup::RunnableGateGroup", connectionPoint.gateId, connectionPoint.connectionEndId);
				copyOldStatesBytecode.push_back(BlockType::BUFFER); // use a buffer to copy the state
				copyOldStatesBytecode.push_back(allocEvalConnectionPointsMain.getIndex(connectionPoint)); // source
				copyOldStatesBytecode.push_back(allocEvalConnectionPointsReserved.getIndex(connectionPoint)); // destination
				calculateGatesBytecode[0]++;
			}
		}

		calculateGatesBytecode[0]++;
		if (gate.type == BlockType::BUFFER || gate.type == BlockType::NOT) {
			// check if the gate has an input
			const auto& connectionsFromPort = gate.getConnectionsFromPort(connection_end_id_t(0));
			if (connectionsFromPort.size() == 0) {
				// treat as constant X
				simulateBytecode.push_back(static_cast<unsigned int>(BlockType::CONSTANT_X)); // block type
			} else {
				assert(connectionsFromPort.size() == 1 && "Buffer/Not gates should have at most one input");
				simulateBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
				unsigned int index = allocEvalConnectionPointsReserved.contains(connectionsFromPort.begin()->first) ? allocEvalConnectionPointsReserved.getIndex(connectionsFromPort.begin()->first) : allocEvalConnectionPointsMain.getIndex(connectionsFromPort.begin()->first);
				simulateBytecode.push_back(index);
			}
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
			nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
		} else if (gate.type == BlockType::AND || gate.type == BlockType::OR || gate.type == BlockType::XOR || gate.type == BlockType::NAND || gate.type == BlockType::NOR || gate.type == BlockType::XNOR) {
			const auto& connectionsFromPort = gate.getConnectionsFromPort(connection_end_id_t(0));
			if (connectionsFromPort.size() == 0) {
				simulateBytecode.push_back(static_cast<unsigned int>(BlockType::CONSTANT_OFF)); // block type
				simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
				nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
				continue;
			}
			simulateBytecode.push_back(static_cast<unsigned int>(gate.type)); // block type
			unsigned int numInputsIndex = simulateBytecode.size();
			simulateBytecode.push_back(0); // num inputs, will be filled in later
			for (const auto& [connectionPoint, weight] : connectionsFromPort) {
				InputOutput direction = gate.getDirection(connection_end_id_t(0), connectionPoint);
				if (direction != InputOutput::INPUT) {
					continue;
				}
				simulateBytecode[numInputsIndex]++;
				unsigned int index = allocEvalConnectionPointsReserved.contains(connectionPoint) ? allocEvalConnectionPointsReserved.getIndex(connectionPoint) : allocEvalConnectionPointsMain.getIndex(connectionPoint);
				simulateBytecode.push_back(index);
			}
			simulateBytecode.push_back(allocEvalConnectionPointsMain.getIndex(EvalConnectionPoint { gate.id, connection_end_id_t(1) }));
			nonJunctionGatesWhoseStateGotWritten.insert(gate.id);
		} else {
			assert(false && "Unsupported gate type in logic group");
		}
	}

	calculateGatesBytecode.insert(calculateGatesBytecode.end(), copyOldStatesBytecode.begin(), copyOldStatesBytecode.end());
	calculateGatesBytecode.insert(calculateGatesBytecode.end(), simulateBytecode.begin(), simulateBytecode.end());

	dataField.resize(dataFieldAllocator);

	logInfo("Group ID: {}", "RunnableGateGroup::RunnableGateGroup", groupId);
	logInfo("dataField size: {}", "RunnableGateGroup::RunnableGateGroup", dataField.size());
	logInfo("pullBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(pullDataBytecode));
	logInfo("calculateBytecode: {}", "RunnableGateGroup::RunnableGateGroup", to_string(calculateGatesBytecode));
	logInfo("publishedStateDataFieldIndices: {}", "RunnableGateGroup::RunnableGateGroup", to_string(publishedStateDataFieldIndices));
}
